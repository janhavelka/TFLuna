# AGENTS.md - TFLunaControl Firmware Engineering Guidelines

## Role
You are a professional embedded software engineer working on production-grade ESP32 systems.

Primary goals:
- Robustness and stability
- Deterministic, predictable behavior
- Portability across boards

Target:
- ESP32-S2 (primary), ESP32-S3 (secondary)
- Arduino framework, PlatformIO

These rules are binding.

---

## Hardware Overview (TFLuna)
- MCU: ESP32-S2 (N4R2)
- SPI: SD card for optional logging
- I2C: ENV sensor (BME280 or SHT31 one-shot), RTC
- E2 bus: EE871 CO2 sensor
- RS485: present, currently stubbed
- Endstops: upper and lower limit inputs (default GPIO5 / GPIO6)
- Status LEDs: WS2812 strip with WiFi and health indicators
- Button: SoftAP control and multi-press reset

---

## Repository Model

```
include/TFLunaControl/   public API headers
src/                  implementation
  main.cpp            firmware entrypoint (board placeholder pins)
  core/               generic logic helpers
  devices/            device adapters
  i2c/                I2C task + orchestrator + bus probes
  logging/            SD logger
  settings/           settings validation + persistence
  web/                SoftAP web API and UI
tests/test_native/    pure logic tests
```

Keep structure predictable and boring.

---

## Core Architecture Rules

### 1) Deterministic Lifecycle
Every subsystem follows:
```cpp
Status begin(const Config& config);
void tick(uint32_t nowMs);
void end();
```

### 2) Cooperative Main Loop
- `TFLunaControl::tick()` is cooperative and bounded.
- No unbounded waits and no `delay()` in library code.
- Long operations are split across ticks or moved to dedicated tasks.
- Async web callbacks must not mutate firmware state directly. Use command queue for mutations and snapshot APIs for reads.

### 3) Explicit Configuration
- All hardware resources come from `Config` / `Settings`.
- No board-specific pin hardcoding in library code.

### 4) Error Model
- All fallible operations return `Status`.
- `Status.msg` must point to static strings only.
- No silent failure paths.

### 5) Memory Discipline
- Allocate in `begin()` only.
- No repeated heap churn in steady-state control paths.
- Use fixed-size queues and ring buffers for runtime pipelines.

---

## I2C Architecture (Task-Owned Bus)

Non-negotiable ownership rule:
- Only `src/i2c/I2cTask` touches the I2C bus via `IdfI2cBackend`.
- No web handler, adapter, or orchestration code accesses the I2C driver directly.

Data flow:
1. `TFLunaControl::tick()` drives `I2cOrchestrator`.
2. `I2cOrchestrator` schedules requests and enqueues them to `I2cTask`.
3. `I2cTask` executes requests in dedicated FreeRTOS task context.
4. Results are queued back and consumed by `I2cOrchestrator` in `tick()` context.
5. Adapters (`EnvSensorAdapter`, `RtcAdapter`) read cached orchestrator state.

Reliability policy:
- Bounded request/result queues.
- Per-request timeout + explicit request deadlines.
- Default backend is ESP-IDF 5.x I2C master driver (deterministic timeout).
- GPIO stuck-bus pre-check before any potentially blocking I2C driver call.
- Late results are tagged and ignored safely by orchestrator state machines.
- Bus recovery uses staged actions with exponential backoff after repeated failures:
  1) backend reset, 2) SCL pulse recovery, 3) optional board power-cycle hook (`Config.i2cPowerCycleHook`).
- Boot-time stuck-SDA detection attempts recovery and reports degraded/fault status.
- Stuck-bus detection and recovery counters exposed in status API.

---

## Adding a New I2C Device Checklist

1. Add scheduling state in `I2cOrchestrator` (poll interval, priority, state machine).
2. Define request pattern (`READ`, `WRITE`, `WRITE_READ`, etc.) and response parsing.
3. Set request timeouts/deadlines explicitly and handle stale results safely.
4. Ensure device cannot starve sensor/RTC requests (respect per-tick request budget).
5. For display-style devices, rate-limit refresh in orchestrator and keep display lower priority than sensors.
6. Include request deadline and stale-result handling so late responses cannot corrupt state.
7. Expose health/status via `DeviceStatus` and API serialization.
8. Add tests in `tests/test_native` for scheduling, timeout, and failure handling.
9. Update docs (`README.md`, this file, summary notes).

---

## I2C Debugging and Recovery Policy

When I2C issues appear:
1. Inspect `/api/status` I2C metrics:
   - error counts, consecutive failures
   - recovery count and last recovery timestamp
   - last recovery stage and power-cycle attempts
   - stuck-SDA count
   - stuck-bus fast-fail count
   - request/result queue depth and drops
   - stale/late result counter
   - slow-op counters and rolling duration max
   - task heartbeat (`i2c_task_alive_ms`) and backend type (`i2c_backend`)
2. Inspect `/api/devices` for RTC/ENV `last_status_*` and timestamps.
3. If consecutive failures persist:
   - verify pull-ups and bus wiring
   - verify configured frequency/timeout
   - reduce frequency and increase timeout via settings
4. Check queue saturation:
   - request overflows or dropped results indicate producer/consumer imbalance
   - lower request rate or increase poll intervals before increasing queue depth

Recovery behavior:
- Repeated failures trigger staged bus recovery with exponential backoff.
- Recovery attempts are rate-limited to avoid thrashing.
- Power-cycle hook is optional and only invoked by escalated recovery stage.
- System stays alive even with I2C fault; web and non-I2C subsystems continue.

---

## Logging Rules
- Library code (`src/`) does not use Serial logging.
- SD logging is queue-based and fault-tolerant.
- Default runtime settings keep SD logging off until the user explicitly enables it.
- `Settings.logFlushMs` is an explicit flush-attempt gate for queued writes.
- Event logging is mandatory for critical transitions (boot, settings, remount, I2C recovery, logger drops/errors).
- Event persistence is bounded (`events.csv` rollover) and retrieval must stay bounded (`/api/events?count=N`).

---

## Web Surface Rules
- Low-level I2C tuning remains CLI-only.
- Web handlers expose operational actions and bounded settings updates, not direct hardware-driver control.

---

## Web JSON Safety Rules
- Do not hand-format JSON with `printf` for API payloads that include strings or floats.
- Use ArduinoJson serialization for endpoint records (`/api/devices`, `/api/graph`, `/api/status`, `/api/settings`) so strings are escaped correctly.
- Any non-finite float (`NaN`, `Inf`) must be emitted as JSON `null` (never raw `nan`/`inf` tokens).
- `ap_pass` is write-only: GET endpoints must never return plaintext password.

---

## Web Endpoint Allocation Policy
- Do not allocate heap per request in high-frequency endpoints.
- `/api/graph` and `/api/events` must use preallocated scratch buffers and fail fast (`503`) when scratch is busy.
- No `new[]`, `malloc`, or unbounded `String` response building in graph/events handlers.
- Preserve bounded response sizes (`MAX_GRAPH_SAMPLES`, `MAX_EVENT_COUNT`) and serializer-based JSON output.

---

## Conformance Gates
Run architecture/security gates locally and in CI:

```bash
python tools/conformance_gates.py
```

Gate coverage:
- I2C ownership rule (`i2c_*` usage outside allowlist fails).
- `ap_pass` exposure rule (plaintext serializer keys fail).
- Web async mutator rule (`_app->` calls in web layer are allowlisted to snapshot/enqueue APIs only).
- Text integrity (BOM/CRLF policy via `scripts/check_text_integrity.py`).

---

## Build and Test

```bash
pio run -e firmware_esp32s2
pio run -e firmware_esp32s3
pio test -e native
```

Optional stress harness build:
- Define `TFLUNACTRL_STRESS_MODE=1` (e.g. extra build flag) to enable controlled command/event churn from `src/main.cpp` for on-target validation.
- This mode is disabled by default and must not be enabled in production release binaries.

---

## Release Gate Checks
Run all checks before release/tag:

```bash
python tools/conformance_gates.py
python scripts/check_text_integrity.py
pio test -e native
pio run -e firmware_esp32s2
pio run -e firmware_esp32s3
rg -n "i2c_master_" src
rg -n "\"ap_pass\"" src include
```

Expected grep results:
- `i2c_master_`: only in `src/i2c/I2cBackend.cpp` (and intentional I2C task/backend code).
- `"ap_pass"`: allowlisted only in:
  - `src/web/WebServer.cpp` POST parse path (`ap_pass_update` + `ap_pass` request key)
  - `src/web/WebPages.h` UI submit payload fields
  - `src/core/SettingsJson.cpp` metadata keys (`ap_pass_set`, `ap_pass_masked`, `ap_pass_update_mode`)
- Plaintext `ap_pass` must never be emitted by any GET endpoint serializer.
- Web server lifecycle must remain reinit-safe (`begin()`/`end()` can be called repeatedly without static singleton state).

Pre-pilot on-target checks (manual):
- SD fault injection (card removal, remount, near-full behavior).
- Web stress with concurrent GET/WS traffic and settings POST under load.
- I2C stuck-line recovery (SDA/SCL low) with fast-fail and staged telemetry validation.

---

## Modification Gate

Before changes:
> Does this increase predictability and portability across projects?

If no, do not proceed.
