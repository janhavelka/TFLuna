# TF-Luna Stability Logger

Focused ESP32-S3 measurement firmware for long-run TF-Luna stability logging.

## Purpose

This project keeps only the pieces needed for:

- TF-Luna LiDAR over UART
- SSD1315 OLED status display
- SoftAP web status page
- SD card CSV logging
- RV3032 RTC timestamps when available

The target use case is unattended overnight logging so distance stability, spread,
signal quality, and timestamp behavior can be checked later from the SD card.

## Preserved Board Wiring

The existing repo wiring assumptions were preserved for the board resources that
were already encoded in firmware:

- I2C SDA: `GPIO8`
- I2C SCL: `GPIO9`
- SD CS: `GPIO10`
- SD MOSI: `GPIO11`
- SD SCK: `GPIO12`
- SD MISO: `GPIO13`

## TF-Luna UART Mapping

The old firmware did not define any `GPIO14/GPIO15` UART mapping, so the TF-Luna
mapping is now explicit in `src/project_config.cpp` and `src/project_config.h`:

- TF-Luna `TX` -> ESP32-S3 `RX` on `GPIO15`
- TF-Luna `RX` -> ESP32-S3 `TX` on `GPIO14`

## Build

```bash
pio run -e firmware_esp32s3
```

## Runtime Output

- OLED shows live reading, validity, strength, counts, min/max, mean, stddev,
  logging state, and RTC vs uptime source.
- Web UI serves `/` and `/api/status` from the built-in SoftAP.
- Serial prints a compact periodic status line.
- SD logging writes CSV rows with:
  - `timestamp`
  - `time_source`
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
