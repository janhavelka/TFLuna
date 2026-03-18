#!/usr/bin/env python3
"""Fail CI when tracked text files contain UTF-8 BOM or CRLF endings."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


TEXT_EXTENSIONS = {
    ".h",
    ".hpp",
    ".c",
    ".cpp",
    ".md",
    ".yml",
    ".yaml",
    ".py",
    ".ini",
    ".json",
}

UTF8_BOM = b"\xEF\xBB\xBF"


def tracked_files(repo_root: Path) -> list[Path]:
    try:
        out = subprocess.check_output(
            ["git", "ls-files"],
            cwd=repo_root,
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except Exception:
        return []
    paths: list[Path] = []
    for raw in out.splitlines():
        if not raw:
            continue
        p = repo_root / raw
        if p.suffix.lower() in TEXT_EXTENSIONS and p.is_file():
            paths.append(p)
    return paths


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    files = tracked_files(repo_root)
    if not files:
        print("No tracked text files to check.")
        return 0

    bom_files: list[Path] = []
    crlf_files: list[Path] = []

    for path in files:
        data = path.read_bytes()
        if data.startswith(UTF8_BOM):
            bom_files.append(path)
        if b"\r\n" in data:
            crlf_files.append(path)

    if not bom_files and not crlf_files:
        print(f"Text integrity check passed ({len(files)} files).")
        return 0

    if bom_files:
        print("UTF-8 BOM detected in:")
        for path in bom_files:
            print(f"  - {path.relative_to(repo_root)}")

    if crlf_files:
        print("CRLF line endings detected in:")
        for path in crlf_files:
            print(f"  - {path.relative_to(repo_root)}")

    return 1


if __name__ == "__main__":
    sys.exit(main())
