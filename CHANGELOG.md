# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-03-20

### Added
- TF-Luna LiDAR acquisition over UART with running distance statistics, health
  counters, and recovery/probe actions
- Task-owned I2C stack for optional ENV (`BME280` or `SHT3x`),
  `RV3032-C7` RTC, and optional `SSD1315` display support
- SoftAP web UI with live status, graphs, devices, endstops, logging, settings,
  and system views
- Serial CLI for diagnostics, recovery, settings, and low-level I2C actions
- SD logging with bounded queues, session folders, rotation, remount support,
  and FAT32 rejection
- Endstop inputs, WS2812 status LED integration, button handling, event logging,
  and native host-side test coverage

### Changed
- Reset the public firmware version history to start at the first actual release
- Audited PlatformIO library pins against current source usage and restored the
  missing `EE871-E2` dependency pin alongside the active sensor, runtime, and
  web libraries
- Runtime build metadata is now generated from the release version declared in
  `library.json`

### Security
- `ap_pass` is write-only in all GET endpoints (plaintext never returned)
- Web JSON uses ArduinoJson serialization (no hand-formatted `printf` for
  strings/floats)
- Non-finite floats (`NaN`, `Inf`) are emitted as JSON `null`

[Unreleased]: https://github.com/janhavelka/TFLuna/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/TFLuna/releases/tag/v1.0.0
