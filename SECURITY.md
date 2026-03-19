# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability within this firmware, please follow responsible disclosure:

1. **Do NOT** open a public GitHub issue.
2. Email the maintainer at: `security@tfluna.local` (replace with actual email).
3. Include:
   - A description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Any suggested fixes (optional)

We will acknowledge receipt within 48 hours and aim to provide a fix or mitigation within 14 days for critical issues.

## Scope

This firmware is designed for embedded systems. Security considerations include:
- No dynamic memory allocation in tick/steady-state paths
- Local SoftAP web UI (no internet connectivity required)
- Persistent storage is opt-in via Config (NVS disabled by default)

## Security Best Practices for Users

- Always validate external inputs before passing to `Config`
- Use hardware watchdogs in production deployments
- Keep dependencies updated
