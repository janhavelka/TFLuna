# TFLunaControl

TFLunaControl is production-oriented TF-Luna firmware for ESP32-S2 and ESP32-S3
targets using Arduino + PlatformIO. The codebase is structured around a
deterministic `begin()` / `tick()` / `end()` lifecycle, a task-owned I2C bus,
bounded queues, and observability surfaces that stay usable even when optional
peripherals fault.

## Current Firmware Surface

- TF-Luna LiDAR acquisition over UART with running distance statistics and
  health counters
- Optional ENV sensor support on the task-owned I2C bus (`BME280` or `SHT31`)
- Optional `RV3032-C7` RTC with cached diagnostics and uptime fallback
- Optional `SSD1315` OLED refresh path on builds that compile display support
- SD card logging with bounded sample/event queues, session folders, rotation,
  remount support, and FAT32 rejection
- SoftAP web UI with Overview, Graphs, Devices, Endstops, Logging, Settings,
  and System tabs
- Serial CLI for diagnostics, recovery, settings, and low-level I2C actions
- Upper and lower endstop inputs, WS2812 status LEDs, and button handling on
  supported board revisions

## Repository Layout

```text
include/TFLunaControl/   public API headers
src/
  main.cpp               firmware entrypoint
  config/                board/app/startup defaults
  core/                  JSON, CLI, scheduler, ring buffers, stats
  devices/               LiDAR, RTC, ENV, LEDs, button, endstops
  i2c/                   task-owned I2C backend, task, orchestrator, probes
  logging/               SD logger
  settings/              validation and persistence
  web/                   SoftAP server and UI
tests/test_native/       host-side logic tests
docs/                    vendor TF-Luna reference PDFs
```

## Default Board Wiring

The sample board mapping is defined in `src/config/AppConfig.cpp`.

- I2C SDA: `GPIO8`
- I2C SCL: `GPIO9`
- SD CS: `GPIO10`
- SD MOSI: `GPIO11`
- SD SCK: `GPIO12`
- SD MISO: `GPIO13`
- TF-Luna `TX` -> ESP32 `RX` on `GPIO15`
- TF-Luna `RX` -> ESP32 `TX` on `GPIO14`
- Upper endstop: `GPIO5`
- Lower endstop: `GPIO6`

ESP32-S3 board-revision extras prepared by the sample config:

- Button: `GPIO38`
- WS2812 status LED data: `GPIO47`

Default board config leaves the EE871 E2 pins disabled. Supply those from your
own application config if you use that bus.

## Build Matrix

`platformio.ini` currently defines these firmware environments:

- `firmware_esp32s3`: default environment, USB CDC on boot, PSRAM enabled, and
  `TFLUNACTRL_ENABLE_DISPLAY=1`
- `firmware_esp32s2`: secondary target, USB CDC on boot, PSRAM enabled, and
  display code left compiled out by the current env flags
- `native`: host-side Unity tests for pure logic and serializer coverage

Build commands:

```bash
pio run -e firmware_esp32s2
pio run -e firmware_esp32s3
```

Validation commands:

```bash
pio test -e native
python tools/conformance_gates.py
python scripts/check_text_integrity.py
```

The native test environment requires a host `gcc/g++` toolchain.

## Runtime Notes

- `src/main.cpp` currently enables `TFLUNACTRL_QUICK_TEST_PROFILE=1` by
  default. On boot, that quick profile forces WiFi on, sets the sample
  interval to `100 ms`, and applies the quick-profile LiDAR/I2C defaults from
  `StartupProfileSettings`. Disable or edit that before release builds.
- `TFLUNACTRL_STRESS_MODE=1` is available for controlled RTC/settings/SD churn,
  but it is off by default and should stay out of production images.
- `AppSettings.enableNvs` is `false` in the sample app. Web and CLI settings
  updates apply immediately, but they are not retained across reboot unless
  your application enables NVS persistence.
- Runtime settings default to a generated `TFLuna-XXXX` SoftAP SSID and the
  password `tflunactrl`. Change that before field deployment.
- SD logging defaults to off. Operators explicitly start or stop it from the
  Logging tab or CLI.
- Low-level I2C tuning is CLI-only. The web layer is limited to operational
  actions and bounded settings changes.

## Web and API Surface

The SoftAP UI is served from `/` and keeps live status current through
WebSocket updates plus periodic full refreshes. Key JSON endpoints are:

- `GET /api/status`
- `GET /api/devices`
- `GET /api/settings`
- `GET /api/settings/defaults`
- `GET /api/graph?count=N`
- `GET /api/events?count=N`
- `GET /api/i2c/raw`
- `GET /api/i2c/scan`
- `POST /api/settings`
- `POST /api/settings/reset`
- `POST /api/device/probe`
- `POST /api/device/recover`
- `POST /api/device/reset_stats`
- `POST /api/sd/remount`
- `POST /api/rtc/set`

Mutation endpoints enqueue commands back into firmware state. They do not reach
through web callbacks into device state directly.

## CLI Surface

The serial CLI starts from `src/main.cpp` when serial support is enabled.
Common entry points:

- `help` or `?`
- `version`
- `diag`
- `device ...`
- `wifi ...`
- `sd ...`
- `rtc ...`
- `env ...`
- `i2c ...`

CLI verbosity accepts named levels: `off`, `normal`, and `verbose`. Numeric
`0..2` is still accepted for compatibility.

## Logging Format

On-device sample CSV files use this header:

```text
timestamp,uptime_ms,sample_index,distance_cm,strength,temperature_c,valid_frame,signal_ok,env_temp_c,env_rh_pct,env_pressure_hpa
```

Event logging is stored separately with bounded rollover. The web Graphs tab
can also export its current in-memory sample view as CSV.

## Time Source Behavior

- If the RTC is readable and valid, logs and APIs use RTC wall-clock time.
- If RTC time is invalid or unavailable, the firmware falls back to uptime and
  exposes that state on serial, web, and status surfaces.
- In sample CSV logs, `timestamp` contains either the wall-clock string or the
  literal `uptime`; `uptime_ms` remains the monotonic reference.

## Included Reference Material

The `docs/` directory contains vendor TF-Luna PDFs bundled for bench bring-up
and protocol reference.
