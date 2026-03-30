#!/usr/bin/env python3
"""
Post OTA binaries to many sensor_id targets using curl.

If the supplied path is a directory, this script uploads:
- node1.bin for sensor_id 1
- node2.bin for sensor_id 2
- ...
Default directory: `var/dist/`.
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
DEFAULT_SENSOR_IDS = [str(i) for i in range(1, 11)]


def load_defaults() -> tuple[str, str]:
    config = configparser.ConfigParser()
    config.read(CONFIG_PATH)
    version = config.get("settings", "firmware_version", fallback="9.9.9")
    ota_token = REDACTED
    upload_token = REDACTED
    token = REDACTED
    return version, token


def parse_sensor_ids(raw: str) -> list[str]:
    if not raw.strip() or raw.strip().lower() == "all":
        return list(DEFAULT_SENSOR_IDS)
    return [part.strip() for part in raw.split(",") if part.strip()]


def resolve_source_path(path_value: str) -> Path:
    path = Path(path_value)
    if not path.is_absolute():
        path = REPO_ROOT / path
    return path


def build_curl_command(url: str, file_path: Path, sensor_id: str, version: str, status: str, token: str, cookie: str) -> list[str]:
    cmd = [
        "curl",
        "--fail-with-body",
        "--location",
        url,
        "-F",
        f"status={status}",
        "-F",
        f"version={version}",
        "-F",
        f"sensor_id={sensor_id}",
        "-F",
        f"file=@{file_path}",
    ]
    if token:
        cmd.extend(["-H", f"Authorization: REDACTED
    if cookie:
        cmd.extend(["-H", f"Cookie: {cookie}"])
    return cmd


def main() -> int:
    default_version, default_token = REDACTED

    parser = argparse.ArgumentParser(description="Upload OTA artifacts to many sensor_id targets with curl.")
    parser.add_argument("path", nargs="?", default="var/dist", help="Firmware file or build output directory (default: var/dist)")
    parser.add_argument("--sensor-ids", default="all", help="Comma-separated sensor_id list, or 'all' (default)")
    parser.add_argument("--version", default=default_version, help=f"Version form field (default: {default_version})")
    parser.add_argument("--status", default="1", help="Status form field for all nodes (default: 1)")
    parser.add_argument("--url", default=DEFAULT_URL, help=f"Upload URL (default: {DEFAULT_URL})")
    parser.add_argument("--token", default=REDACTED
    parser.add_argument("--cookie", default="", help="Optional Cookie header value")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them")
    args = parser.parse_args()

    source_path = resolve_source_path(args.path)
    if not source_path.exists():
        print(f"Path not found: {source_path}", file=sys.stderr)
        return 1

    sensor_ids = parse_sensor_ids(args.sensor_ids)
    if not sensor_ids:
        print("No sensor_id values selected.", file=sys.stderr)
        return 1

    exit_code = 0
    for sensor_id in sensor_ids:
        file_path = source_path / f"node{sensor_id}.bin" if source_path.is_dir() else source_path
        if not file_path.exists():
            print(f"[FAILED] sensor_id={sensor_id} missing firmware: {file_path}", file=sys.stderr)
            exit_code = 1
            continue
        cmd = build_curl_command(args.url, file_path, sensor_id, args.version, args.status, args.token, args.cookie)
        print(f"[POST] sensor_id={sensor_id} version={args.version} status={args.status}")
        print(" ".join(cmd))
        if args.dry_run:
            continue
        result = subprocess.run(cmd, cwd=REPO_ROOT)
        if result.returncode != 0:
            exit_code = result.returncode
            print(f"[FAILED] sensor_id={sensor_id}", file=sys.stderr)
        else:
            print(f"[OK] sensor_id={sensor_id}")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
