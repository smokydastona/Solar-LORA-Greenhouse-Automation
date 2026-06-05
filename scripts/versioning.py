from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION_HEADER = REPO_ROOT / "include" / "Version.h"


def _git_output(args: list[str], fallback: str) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=REPO_ROOT, text=True).strip()
    except Exception:
        return fallback


def read_firmware_series() -> str:
    try:
        header = VERSION_HEADER.read_text(encoding="utf-8")
    except FileNotFoundError:
        return "0.1"

    match = re.search(r'kFirmwareSeries\s*=\s*"([^"]+)"', header)
    if match:
        return match.group(1)

    legacy_match = re.search(r'kFirmwareVersion\s*=\s*"([^"]+)"', header)
    if legacy_match:
        parts = legacy_match.group(1).split(".")
        if len(parts) >= 2:
            return ".".join(parts[:2])
    return "0.1"


def resolve_build_metadata() -> tuple[str, str, str]:
    series = read_firmware_series()
    commit_count = _git_output(["rev-list", "--count", "HEAD"], "0")
    short_sha = os.environ.get("GITHUB_SHA", "")[:8] or _git_output(["rev-parse", "--short=8", "HEAD"], "nogit")
    dirty_suffix = ""
    if _git_output(["status", "--porcelain"], ""):
        dirty_suffix = "-dirty"

    version = f"{series}.{commit_count}"
    label = f"g{short_sha}{dirty_suffix}"
    return series, version, label


def main() -> int:
    parser = argparse.ArgumentParser(description="Resolve the automatic Mini Greenhouse firmware version.")
    parser.add_argument("--field", choices=["series", "version", "label"], default="version")
    parser.add_argument("--github-output", help="Optional path to a GitHub Actions output file.")
    args = parser.parse_args()

    series, version, label = resolve_build_metadata()
    values = {"series": series, "version": version, "label": label}

    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as handle:
            handle.write(f"series={series}\n")
            handle.write(f"version={version}\n")
            handle.write(f"label={label}\n")

    print(values[args.field])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())