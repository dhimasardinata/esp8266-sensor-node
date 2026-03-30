#!/usr/bin/env python3
"""
Post one OTA binary to the TA files API using curl.

If the supplied path is a directory, this script uploads `node<SENSOR_ID>.bin`
from that directory. Default directory: `var/dist/`.
"""

from __future__ import annotations

import argparse
import configparser
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = REPO_ROOT / "node_id.ini"
DEFAULT_URL = "https://example.com/api/files"


def load_defaults() -> tuple[str, str]:
    config = configparser.ConfigParser()
    config.read(CONFIG_PATH)
    version = config.get("settings", "firmware_version", fallback="9.9.9")
    ota_token = REDACTED
    upload_token = REDACTED
    token = REDACTED
    return version, token


def resolve_file(path_value: str, sensor_id: str) -> Path:
    path = Path(path_value)
    if not path.is_absolute():
        path = REPO_ROOT / path
    if path.is_dir():
        path = path / f"node{sensor_id}.bin"
    return path


def main() -> int:
    default_version, default_token = REDACTED

    parser = argparse.ArgumentParser(description="Upload one OTA artifact with curl.")
    parser.add_argument("path", nargs="?", default="var/dist", help="Firmware file or build output directory (default: var/dist)")
    parser.add_argument("--sensor-id", required=True, help="Node sensor_id value")
    parser.add_argument("--version", default=default_version, help=f"Version form field (default: {default_version})")
    parser.add_argument("--status", default="1", help="Status form field (default: 1)")
    parser.add_argument("--url", default=DEFAULT_URL, help=f"Upload URL (default: {DEFAULT_URL})")
    parser.add_argument("--token", default=REDACTED
    parser.add_argument("--cookie", default="", help="Optional Cookie header value")
    parser.add_argument("--dry-run", action="store_true", help="Print curl command without executing it")
    args = parser.parse_args()

    file_path = resolve_file(args.path, args.sensor_id)
    if not file_path.exists():
        print(f"File not found: {file_path}", file=sys.stderr)
        return 1

    cmd = [
        "curl",
        "--fail-with-body",
        "--location",
        args.url,
        "-F",
        f"status={args.status}",
        "-F",
        f"version={args.version}",
        "-F",
        f"sensor_id={args.sensor_id}",
        "-F",
        f"file=@{file_path}",
    ]

    if args.token:
        cmd.extend(["-H", f"Authorization: REDACTED
    if args.cookie:
        cmd.extend(["-H", f"Cookie: {args.cookie}"])

    print(" ".join(cmd))
    if args.dry_run:
        return 0

    result = subprocess.run(cmd, cwd=REPO_ROOT)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
