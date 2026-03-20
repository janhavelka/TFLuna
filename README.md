# TFLunaControl

ESP32-S2/S3 TF-Luna measurement firmware with a deterministic app lifecycle,
SSD1315 OLED status display, RV3032 RTC support, opt-in SD logging, SoftAP web
UI, serial CLI diagnostics, and endstop inputs.

## Purpose

This firmware is centered around stable TF-Luna acquisition and observability:

- TF-Luna LiDAR over UART
- SSD1315 OLED live status display
- SoftAP web dashboard with overview, graphs, devices, endstops, logging, settings, and system tabs
- SD card CSV and event logging
- RV3032 RTC timestamps when available
- Upper and lower limit inputs

The main use case is unattended distance measurement with enough local telemetry
to diagnose timing, signal quality, logging health, and time-source behavior.

## Default Board Wiring

The default board mapping is defined in `src/config/AppConfig.cpp`.

- I2C SDA: `GPIO8`
- I2C SCL: `GPIO9`
- SD CS: `GPIO10`
- SD MOSI: `GPIO11`
- SD SCK: `GPIO12`
- SD MISO: `GPIO13`
- TF-Luna `TX` -> ESP32 `RX` on `GPIO15`
- TF-Luna `RX` -> ESP32 `TX` on `GPIO14`
- Upper limit input: `GPIO5`
- Lower limit input: `GPIO6`

Board-revision-specific S3 defaults are also prepared there:

- Button: `GPIO38`
- Status LED: `GPIO47`

## Build

```bash
pio run -e firmware_esp32s2
pio run -e firmware_esp32s3
```

## Test

```bash
pio test -e native
python tools/conformance_gates.py
python scripts/check_text_integrity.py
```

The native test environment requires a host `gcc/g++` toolchain.

## Runtime Behavior

- OLED shows live distance, validity, strength, running distance stats, logging
  state, and RTC vs uptime source.
- Web UI serves `/` plus JSON APIs and keeps live status current over WebSocket.
- SD logging is off by default after reset/restore defaults. The user starts
  and stops logging explicitly from the Logging tab or CLI.
- Low-level I2C tuning is CLI-only. The web UI is limited to operational
  actions and non-I2C settings.
- CLI verbosity uses named levels: `off`, `normal`, `verbose`.
  Numeric `0..2` is still accepted for compatibility.

## Logged Sample Fields

Sample CSV rows include:

- `timestamp`
- `uptime_ms`
- `sample_index`
- `distance_cm`
- `strength`
- `temperature_c`
- `valid_frame`
- `signal_ok`

## Time Source Behavior

- If the RV3032 RTC is readable and valid, logs use RTC wall-clock timestamps.
- If RTC time is invalid or unavailable, the firmware falls back to uptime-based
  timestamps and exposes that fallback on serial, web, OLED, and CSV rows.
- In CSV logs, `timestamp` contains either the RTC wall-clock string or the
  literal `uptime`; `uptime_ms` remains available as the monotonic time base.
