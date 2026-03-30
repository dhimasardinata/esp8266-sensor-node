#!/usr/bin/env python3
"""
Build firmware binaries and upload them to the OTA files API in one command.

Default flow:
- build node 1..10 into var/dist/node1.bin .. var/dist/node10.bin
- upload those same files to https://example.com/api/files

This wrapper is intentionally flexible:
- build and upload can target the same nodes, or different ones
- build version and OTA-posted version can be overridden separately
- upload can reuse a different directory/file than the current build output
- either phase can be skipped when needed
"""

from __future__ import annotations

import argparse
import configparser
import shlex
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = REPO_ROOT / "node_id.ini"
DEFAULT_OUT_DIR = "var/dist"
DEFAULT_URL = "https://example.com/api/files"


def load_defaults() -> tuple[str, str]:
    config = configparser.ConfigParser()
    config.read(CONFIG_PATH)
    version = config.get("settings", "firmware_version", fallback="9.9.9")
    ota_token = REDACTED
    upload_token = REDACTED
    token = REDACTED
    return version, token


def print_command(label: str, command: list[str]) -> None:
    print(label)
    print(" ".join(shlex.quote(part) for part in command))


def main() -> int:
    default_version, default_token = REDACTED

    parser = argparse.ArgumentParser(description="Build node firmware and upload OTA artifacts in one go.")
    parser.add_argument("--version", default=default_version, help=f"Common default version for build/upload (default: {default_version})")
    parser.add_argument("--build-version", default=None, help="Override firmware version for the build step only")
    parser.add_argument("--ota-version", default=REDACTED
    parser.add_argument("--nodes", default="all", help="Common default node list for build/upload, comma-separated or 'all'")
    parser.add_argument("--build-nodes", default=None, help="Override node list for build step only")
    parser.add_argument("--sensor-ids", default=None, help="Override upload sensor_id list for OTA step only")
    parser.add_argument("--out", default=DEFAULT_OUT_DIR, help=f"Build output directory (default: {DEFAULT_OUT_DIR})")
    parser.add_argument("--upload-path", default=None, help="Directory or firmware file for OTA upload (default: same as --out)")
    parser.add_argument("--env", default="wemosd1mini_usb", help="PlatformIO environment (default: wemosd1mini_usb)")
    parser.add_argument("--pio", default=None, help="Path to pio/platformio executable for build step")
    parser.add_argument("--status", default="1", help="OTA status form field for all uploaded nodes (default: 1)")
    parser.add_argument("--url", default=DEFAULT_URL, help=f"OTA upload URL (default: {DEFAULT_URL})")
    parser.add_argument("--token", default=REDACTED
    parser.add_argument("--cookie", default="", help="Optional Cookie header value for OTA upload")
    parser.add_argument("--build-only", action="store_true", help="Only run build step")
    parser.add_argument("--upload-only", action="store_true", help="Only run upload step")
    parser.add_argument("--dry-run", action="store_true", help="Backward-compatible alias for --upload-dry-run")
    parser.add_argument("--upload-dry-run", action="store_true", help="Print curl upload commands without executing the upload step")
    args = parser.parse_args()

    if args.build_only and args.upload_only:
        print("Cannot combine --build-only and --upload-only.", file=sys.stderr)
        return 1

    build_nodes = args.build_nodes or args.nodes
    sensor_ids = args.sensor_ids or args.nodes
    build_version = args.build_version or args.version
    ota_version = REDACTED
    upload_path = args.upload_path or args.out
    upload_dry_run = args.upload_dry_run or args.dry_run

    build_cmd = [
        sys.executable,
        str(REPO_ROOT / "scripts" / "build_all.py"),
        "--env",
        args.env,
        "--version",
        build_version,
        "--nodes",
        build_nodes,
        "--out",
        args.out,
    ]
    if args.pio:
        build_cmd.extend(["--pio", args.pio])

    upload_cmd = [
        sys.executable,
        str(REPO_ROOT / "REDACTED" / "REDACTED"),
        upload_path,
        "--sensor-ids",
        sensor_ids,
        "--version",
        ota_version,
        "--status",
        args.status,
        "--url",
        args.url,
    ]
    if args.token:
        upload_cmd.extend(["REDACTED", args.token])
    if args.cookie:
        upload_cmd.extend(["--cookie", args.cookie])
    if upload_dry_run:
        upload_cmd.append("--dry-run")

    if not args.upload_only:
        print_command("[STEP 1/2] Build firmware binaries", build_cmd)
        subprocess.run(build_cmd, cwd=REPO_ROOT, check=True)

    if not args.build_only:
        print_command("REDACTED", upload_cmd)
        subprocess.run(upload_cmd, cwd=REPO_ROOT, check=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
