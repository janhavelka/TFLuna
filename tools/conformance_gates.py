#!/usr/bin/env python3
"""Architecture/security conformance gates for TFLunaControl."""

from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_EXTENSIONS = {".h", ".hpp", ".c", ".cpp"}


@dataclass
class Failure:
    gate: str
    path: Path
    line: int
    message: str
    text: str


def iter_source_files(roots: Iterable[str]) -> Iterable[Path]:
    for root in roots:
        base = REPO_ROOT / root
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in SOURCE_EXTENSIONS:
                yield path


def relative(path: Path) -> Path:
    return path.relative_to(REPO_ROOT)


def read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def gate_i2c_ownership() -> list[Failure]:
    gate = "I2C ownership"
    allowlist = {Path("src/i2c/I2cBackend.cpp")}
    patterns = [
        (re.compile(r"\bWire\."), "forbidden Wire API usage (Wire backend removed)"),
        (re.compile(r"\bi2c_[A-Za-z0-9_]*\s*\("), "forbidden ESP-IDF i2c_* usage outside allowlist"),
    ]

    failures: list[Failure] = []
    for path in iter_source_files(("src", "include")):
        rel = relative(path)
        if rel in allowlist:
            continue
        for line_no, line in enumerate(read_lines(path), start=1):
            for pattern, message in patterns:
                if pattern.search(line):
                    failures.append(Failure(gate, rel, line_no, message, line))
    return failures


def gate_ap_pass_exposure() -> list[Failure]:
    gate = "ap_pass exposure"
    allowed_files = {
        Path("src/web/WebServer.cpp"),
        Path("src/web/WebPages.h"),
        Path("src/core/SettingsJson.cpp"),
    }
    exact_plain_key = re.compile(r'"ap_pass"')
    serializer_assign = re.compile(r'\[\s*"ap_pass"\s*\]\s*=')

    failures: list[Failure] = []
    for path in iter_source_files(("src", "include")):
        rel = relative(path)
        for line_no, line in enumerate(read_lines(path), start=1):
            if "ap_pass" not in line:
                continue

            if rel not in allowed_files:
                failures.append(
                    Failure(
                        gate,
                        rel,
                        line_no,
                        "ap_pass token found outside allowlist",
                        line,
                    )
                )
                continue

            if rel == Path("src/core/SettingsJson.cpp") and exact_plain_key.search(line):
                failures.append(
                    Failure(
                        gate,
                        rel,
                        line_no,
                        "plaintext ap_pass key is forbidden in GET serializer",
                        line,
                    )
                )

            if rel == Path("src/web/WebServer.cpp") and serializer_assign.search(line):
                failures.append(
                    Failure(
                        gate,
                        rel,
                        line_no,
                        "serializer must not assign ap_pass in any response payload",
                        line,
                    )
                )
    return failures


def gate_async_web_mutators() -> list[Failure]:
    gate = "Web async mutator rule"
    allowed_calls = {
        "tryGetStatusSnapshot",
        "tryCopyDeviceStatuses",
        "tryGetSettingsSnapshot",
        "tryGetI2cRawSnapshot",
        "tryGetI2cScanSnapshot",
        "tryCopySamples",
        "tryCopyEvents",
        "enqueueApplySettings",
        "enqueueSetRtcTime",
        "enqueueRemountSd",
        "enqueueSetOutputChannelTest",
        "enqueueProbeSdCard",
        "enqueueRecoverI2cBus",
        "enqueueRecoverLidarSensor",
        "enqueueProbeLidarSensor",
        "enqueueScanI2cBus",
        "enqueueI2cProbeAddress",
    }
    app_call = re.compile(r"_app->([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    nonblocking_take = re.compile(r"xSemaphoreTake\s*\([^,]+,\s*0[UuLl]*\s*\)")
    any_take = re.compile(r"xSemaphoreTake\s*\(")

    failures: list[Failure] = []
    web_root = REPO_ROOT / "src" / "web"
    if not web_root.exists():
        return failures

    for path in web_root.rglob("*.cpp"):
        rel = relative(path)
        for line_no, line in enumerate(read_lines(path), start=1):
            for match in app_call.finditer(line):
                method = match.group(1)
                if method not in allowed_calls:
                    failures.append(
                        Failure(
                            gate,
                            rel,
                            line_no,
                            f"_app->{method}() is not allowed in web layer; use snapshot/enqueue API",
                            line,
                        )
                    )
            if any_take.search(line) and not nonblocking_take.search(line):
                failures.append(
                    Failure(
                        gate,
                        rel,
                        line_no,
                        "blocking xSemaphoreTake in web layer is forbidden",
                        line,
                    )
                )
    return failures


def gate_text_integrity() -> list[Failure]:
    gate = "Text integrity"
    cmd = [sys.executable, str(REPO_ROOT / "scripts" / "check_text_integrity.py")]
    proc = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode == 0:
        return []

    output = (proc.stdout + "\n" + proc.stderr).strip().splitlines()
    failures = []
    for idx, line in enumerate(output, start=1):
        failures.append(Failure(gate, Path("scripts/check_text_integrity.py"), idx, "text integrity check failed", line))
    return failures


def print_gate_result(name: str, failures: list[Failure]) -> None:
    if not failures:
        print(f"[PASS] {name}")
        return
    print(f"[FAIL] {name} ({len(failures)} issue(s))")
    for failure in failures:
        print(
            f"  - {failure.path}:{failure.line}: {failure.message}\n"
            f"    {failure.text.strip()}"
        )


def main() -> int:
    gates: list[tuple[str, list[Failure]]] = [
        ("I2C ownership", gate_i2c_ownership()),
        ("ap_pass exposure", gate_ap_pass_exposure()),
        ("Web async mutator rule", gate_async_web_mutators()),
        ("Text integrity", gate_text_integrity()),
    ]

    any_fail = False
    for name, failures in gates:
        print_gate_result(name, failures)
        any_fail = any_fail or bool(failures)

    if any_fail:
        print("\nConformance gates failed.")
        return 1
    print("\nConformance gates passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
