# CO2Control Firmware (ESP32-S2 / ESP32-S3)

Production-grade firmware for **CO2Control / Pavel Reiterman Ustredna**. The project follows a deterministic **begin/tick/end** lifecycle with a unified **Status/Health** model, robust SD logging, and a local SoftAP web UI.

## Quickstart

```bash
# Build for ESP32-S2
pio run -e firmware_esp32s2

# Upload + monitor
pio run -e firmware_esp32s2 -t upload
pio device monitor -e firmware_esp32s2
```

## Supported Targets

| Board                 | Environment         | Notes           |
| --------------------- | ------------------- | --------------- |
| ESP32-S2 (Saola-1)    | `firmware_esp32s2`   | Primary target  |
| ESP32-S3 (DevKitC-1)  | `firmware_esp32s3`   | Secondary check |

## Dependency Pins

External runtime libraries are pinned in [`platformio.ini`](./platformio.ini):

- `BME280` `v1.2.1`
- `SHT3x-main` `v1.4.0`
- `RV3032-C7` `v1.3.0`
- `SSD1315` `v1.1.0`
- `EE871-E2` `v0.3.0`
- `AsyncSD` `v1.3.0`
- `SystemChrono` `v1.2.0`
- `StatusLED` `v1.3.0`

## Architecture

### Lifecycle

```cpp
#include "CO2Control/CO2Control.h"

CO2Control::CO2Control app;

void setup() {
  CO2Control::Config cfg;
  CO2Control::Status st = app.begin(cfg);
  if (!st.ok()) {
    // handle error
  }
}

void loop() {
  app.tick(millis());
}
```

### Threading Model
- Main orchestration runs in cooperative `loop()` via `tick()`.
- Async web handlers never mutate app state directly; they enqueue commands.
- Web reads use non-blocking snapshots and return `503` when state is busy.
- I2C runs in a dedicated FreeRTOS task; only that task touches the bus driver.
- RV3032 RTC and SHT3x/BME280 sensor libraries are integrated through task-owned I2C callbacks.
- Default I2C backend on ESP32-S2/S3 is ESP-IDF 5.x I2C master driver (deterministic timeout).

### Timing
- `tick()` is cooperative and bounded by per-component work limits.
- SD logger processes at most one stream operation per tick and tracks per-tick I/O budget via `Settings.logIoBudgetMs`.
- I2C request generation is budgeted per tick (`Settings.i2cRequestsPerTick`), and stale results are ignored safely by token/deadline matching.
- I2C task performs GPIO preflight stuck-bus checks before every transaction. If SDA/SCL is held low, request fails fast with `BUS_STUCK` and recovery backoff is scheduled without entering the backend call.
- I2C op timeout target is controlled by `Settings.i2cOpTimeoutMs`; IDF backend enforces this through driver timeout ticks.

### Resource Ownership
- UART/I2C/SPI pins are provided via `Config`. No hardcoded pins in library code.
- `src/main.cpp` is the firmware entrypoint and board placeholder for pin mapping.

### Memory
- All allocation occurs in `begin()`. Zero allocations in `tick()`.

### Error Handling
- All fallible APIs return `Status`. No silent failures.

## Configuration

### Hardware Config
Hardware settings live in `include/CO2Control/HardwareSettings.h`. Board-specific defaults/placeholders are set in `src/main.cpp`.

### Settings
Runtime settings live in `include/CO2Control/RuntimeSettings.h` and can be loaded/saved via NVS when enabled. Defaults:
- `sampleIntervalSec = 600`
- `logDailyEnabled = true`
- `logAllEnabled = true`
- `wifiEnabled = false`
- `apSsid = "CO2Control-XXXX"`
- `apPass = "co2control"`
- `logIoBudgetMs = 10`
- `logSessionName = "run"` (session directories under `/logs/runs`, e.g. `run_00001`)
- `i2cFreqHz = 400000`
- `i2cOpTimeoutMs = 20`
- `i2cSlowOpThresholdUs = 50000`
- `i2cRtcAddress = 0x51` (RV3032-C7 default)
- `i2cRtcBackupMode = 1` and `i2cRtcEnableEepromWrites = true` so RTC backup configuration persists across complete power loss by default

Build-time feature flags (in `platformio.ini`):
- `CO2CONTROL_ENABLE_WEB` (`1` by default)
- `CO2CONTROL_ENABLE_DISPLAY` (`0` by default, disables all OLED refresh scheduling)

## Serial CLI

The CLI follows grouped help/output conventions:

- `help` or `?` prints grouped command overview.
- `help <topic>` prints detailed contract for a single domain.
- Key health/error tokens are color-emphasized for faster diagnostics (`OK`, `DEGRADED`, `FAULT`, errors).
- `version` prints firmware version and linked library versions.

## Logging (SD)

Two outputs are written in parallel:
- `logs/daily/YYYY-MM-DD.csv`
- `logs/all.csv`

CSV format:
```
ts_unix,ts_local,co2_ppm,temp_c,rh_pct,pressure_hpa,valid_mask
```

If `logs/all.csv` exceeds the size limit (default 3.5 GB), it stops growing and is marked degraded while daily logs continue.
Firmware uses `AsyncSD` (pinned in `platformio.ini`) for bounded, nonblocking SD I/O.
Mount flow creates `/logs` + `/logs/daily` and verifies `/logs` via `requestListDir()` before marking SD ready.
Manual `POST /api/sd/remount` uses AsyncSD remount request flow (unmount+remount in-place) without tearing down the worker.
`logFlushMs` controls the minimum spacing between queued SD flush attempts.
Events are persisted to `logs/events.csv` with rollover to `logs/events.prev.csv` when the file reaches the bounded size limit.
Logger status in `/api/status` includes:
- `log_io_budget_ms`
- `log_last_tick_elapsed_ms`
- `log_budget_exceeded_count`
- `log_last_error_age_ms`
- `log_event_queue_depth`
- `log_event_dropped_count`

FAT32 compatibility notes:
- Large FAT32 cards (16-32 GB) require a full FAT table scan to report free space.
  This scan can take 10-30 seconds depending on card size and speed.
  During the scan the worker task is blocked and file-write operations queued behind
  it may time out once; they are retried automatically on the next tick.
- Default SPI frequency is 4 MHz for maximum card compatibility. Higher frequencies
  (up to 25 MHz) can be set via `AppSettings::sdSpiFrequencyHz` for faster throughput
  once the card model has been validated.
- Info refresh is rate-limited to once per minute (success) with exponential backoff
  after failures, so the slow scan does not compete with write I/O.

## I2C Recovery and Metrics

I2C recovery uses staged escalation with backoff:
1. backend reset (`end/begin`)
2. optional SCL pulse sequence (`Config.i2cRecoveryPulses`)
3. optional board-specific power-cycle hook

`/api/status` exports production diagnostics:
- bus errors, recoveries, last recovery stage
- stuck-bus fast-fail counter
- request/result queue depth and overflow/drop counters
- stale result counter
- op duration stats (max, rolling max, mean, slow-op counters)
- task heartbeat and backend type (`idf`)

## Outputs (Valve + Fan)

Outputs are role-based:
- Valve channel (`output_valve_channel`) is controlled by hysteresis source (`output_source`: CO2 or temperature)
- Fan channel (`output_fan_channel`) uses configured power (`output_fan_pwm_percent`)

Valve control:
- ON at `co2_on_ppm` / `temp_on_c` (based on source)
- OFF at `co2_off_ppm` / `temp_off_c`
- `min_on_ms` / `min_off_ms` enforce minimum dwell time

Fan control:
- `0%` = off
- `1..100%` maps to effective `30..100%` on MOSFET channels
- Relay channels are on/off only (any value `>0` means ON)

## Web API (SoftAP)

Endpoints:
- `GET /api/status`
- `GET /api/devices`
- `GET /api/settings`
- `POST /api/settings`
- `GET /api/graph?count=150`
- `GET /api/events?count=64`
- `POST /api/sd/remount`
- `POST /api/rtc/set`

WebSocket:
- `/ws` for live status updates

`GET /api/settings` never returns plaintext `ap_pass`. Password updates are write-only via `POST /api/settings` with:
- `ap_pass_update: true`
- `ap_pass: "<new password>"`

## Status LEDs

Two WS2812 LEDs on a shared strip:
- WiFi/Web LED
  - Red solid: AP off
  - Blue blink: AP on, no stations
  - Blue solid: station connected
  - Green solid: web client active
- Health LED
  - Green blink: init
  - Amber solid: DEGRADED
  - Red/blue police blink: any FAULT
  - Green solid: OK

## Tests

Native logic tests:
```bash
pio test -e native
```

## Versioning

`include/CO2Control/Version.h` is auto-generated from `library.json` before each build by `scripts/generate_version.py`. Do not edit it manually.

## Copyright

Copyright (c) 2026 Jan Havelka, Thymos Solution s.r.o (www.thymos.cz, info@thymos.cz)
