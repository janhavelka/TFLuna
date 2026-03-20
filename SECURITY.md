# Security Policy

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 1.0.x   | Yes       |
| < 1.0.0 | No        |

## Reporting a Vulnerability

Do not disclose vulnerabilities in a public GitHub issue.

Prefer one of these private paths:

1. Use GitHub's private vulnerability reporting for the repository, if enabled.
2. If that is not available, contact the maintainer through a private channel
   linked from the maintainer's GitHub profile before publishing details.

Include:

- affected firmware version or commit
- target board (`ESP32-S2` or `ESP32-S3`)
- steps to reproduce
- impact and expected attacker position
- logs, captures, or proof-of-concept details as needed

Please keep details private until a fix or mitigation is ready.

## Scope

Relevant security surfaces in this firmware include:

- SoftAP HTTP and WebSocket endpoints
- serial CLI commands and parsing
- settings persistence and secret handling
- SD logging and exported diagnostics
- local bus inputs (UART, I2C, SPI, button/endstop GPIO) treated as untrusted

Project invariants worth preserving:

- `ap_pass` is write-only on GET endpoints
- JSON responses are serialized, not hand-formatted
- non-finite floats are emitted as JSON `null`
- web callbacks do not mutate hardware state directly

## Deployment Notes

- Change the default SoftAP password before field use.
- Disable WiFi entirely when a deployment does not need local web access.
- Leave NVS persistence disabled unless retained settings are required.
- Keep third-party libraries and PlatformIO platform pins current.
