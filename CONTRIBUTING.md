# Contributing

This project targets production-grade ESP32-S2 and ESP32-S3 firmware. Favor
predictable behavior, bounded runtime work, and portability over cleverness.

## Workflow

1. Fork the repository and create a focused branch.
2. Make the smallest change that fully solves the problem.
3. Update docs and `CHANGELOG.md` when behavior, defaults, or interfaces move.
4. Run the local validation suite before opening a PR.

## Required Validation

```bash
python tools/conformance_gates.py
python scripts/check_text_integrity.py
pio test -e native
pio run -e firmware_esp32s2
pio run -e firmware_esp32s3
```

If your change affects hardware behavior, include any manual bench validation in
the PR description.

## Architecture Expectations

- Preserve the deterministic subsystem lifecycle: `begin()`, `tick()`, `end()`.
- Keep `TFLunaControl::tick()` cooperative and bounded. Do not add blocking
  waits or `delay()` calls in library code.
- Only `src/i2c/I2cTask` may touch the I2C backend directly.
- Web callbacks must enqueue mutations; read paths should use snapshots.
- Avoid heap churn in steady-state control paths.
- `Status.msg` must always point to static strings.
- Do not add `Serial` logging inside `src/`; use existing telemetry surfaces.

## Tests and Docs

- Add or extend tests under `tests/test_native` for logic, scheduling, timeout,
  serialization, or validation changes.
- Keep `README.md`, `CHANGELOG.md`, and any affected operator/developer docs in
  sync with the code.
- Preserve JSON safety rules: no plaintext `ap_pass` on GET responses and no
  hand-built JSON payloads for string/float-bearing endpoints.

## Commits

Use [Conventional Commits](https://www.conventionalcommits.org/) where practical:

- `feat:` new feature
- `fix:` bug fix
- `docs:` documentation-only change
- `refactor:` internal code change without user-visible behavior change
- `test:` test-only change
- `chore:` maintenance

## Pull Requests

- Keep PRs narrow and explain the user-visible impact.
- Call out target board assumptions, risk areas, and any manual test coverage.
- Add a changelog entry under `[Unreleased]`.
- Do not merge changes that break the conformance gates or either firmware
  target.

## Questions

Open a GitHub issue for bugs or a discussion thread for general design
questions.
