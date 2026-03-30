#!/usr/bin/env python3
"""
Build node firmware outputs without overwriting each other.

Default behavior:
- build node 1..10
- map node 1..5  -> GH 1
- map node 6..10 -> GH 2
- write outputs to var/dist/node1.bin .. var/dist/node10.bin

The selected firmware version is injected for the build, then the repo config/header
is restored so the worktree stays clean.
"""

from __future__ import annotations

import argparse
import configparser
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = REPO_ROOT / "node_id.ini"
HEADER_PATH = REPO_ROOT / "include" / "generated/node_config.h"
DEFAULT_OUT_DIR = REPO_ROOT / "var" / "dist"
DEFAULT_ENV = "wemosd1mini_usb"
DEFAULT_VERSION = "9.9.9"
DEFAULT_NODES = list(range(1, 11))
PIO_CANDIDATES = [
    "pio",
    "platformio",
    str(Path.home() / ".platformio" / "penv" / "bin" / "pio"),
    str(Path.home() / ".platformio" / "penv" / "bin" / "platformio"),
]


def load_config(path: Path) -> configparser.ConfigParser:
    config = configparser.ConfigParser()
    config.read(path)
    if "settings" not in config:
        config["settings"] = {}
    return config


def save_config(path: Path, config: configparser.ConfigParser) -> None:
    with path.open("w", encoding="utf-8") as fh:
        config.write(fh)


def find_pio(explicit: str | None) -> str:
    if explicit:
        return explicit

    for candidate in PIO_CANDIDATES:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
        if Path(candidate).exists():
            return candidate

    raise FileNotFoundError("PlatformIO executable not found. Use --pio /path/to/pio")


def gh_for_node(node_id: int) -> int:
    if 1 <= node_id <= 5:
        return 1
    if 6 <= node_id <= 10:
        return 2
    raise ValueError(f"Node {node_id} out of supported range 1..10")


def parse_nodes(raw: str) -> list[int]:
    if not raw.strip() or raw.strip().lower() == "all":
        return list(DEFAULT_NODES)

    nodes: list[int] = []
    for chunk in raw.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        node_id = int(chunk)
        if not 1 <= node_id <= 10:
            raise ValueError(f"Node {node_id} out of supported range 1..10")
        nodes.append(node_id)
    return nodes


def backup_file(path: Path) -> str | None:
    if not path.exists():
        return None
    return path.read_text(encoding="utf-8")


def restore_file(path: Path, content: str | None) -> None:
    if content is None:
        if path.exists():
            path.unlink()
        return
    path.write_text(content, encoding="utf-8")


def run_build(pio: str, env_name: str) -> None:
    subprocess.run([pio, "run", "-e", env_name], cwd=REPO_ROOT, check=True)


def main() -> int:
    default_config = load_config(CONFIG_PATH)
    default_version = default_config.get("settings", "firmware_version", fallback=DEFAULT_VERSION)

    parser = argparse.ArgumentParser(description="Build node1.bin..node10.bin with automatic GH mapping.")
    parser.add_argument("--version", default=default_version, help=f"Firmware version to inject (default: {default_version})")
    parser.add_argument("--env", default=DEFAULT_ENV, help=f"PlatformIO environment (default: {DEFAULT_ENV})")
    parser.add_argument("--out", default=str(DEFAULT_OUT_DIR), help=f"Output directory (default: {DEFAULT_OUT_DIR.relative_to(REPO_ROOT)})")
    parser.add_argument("--nodes", default="all", help="Comma-separated node list, or 'all' (default)")
    parser.add_argument("--pio", default=None, help="Path to pio/platformio executable")
    args = parser.parse_args()

    pio = find_pio(args.pio)
    nodes = parse_nodes(args.nodes)

    out_dir = Path(args.out)
    if not out_dir.is_absolute():
        out_dir = REPO_ROOT / out_dir

    config = load_config(CONFIG_PATH)
    settings = config["settings"]
    original_config = backup_file(CONFIG_PATH)
    original_header = backup_file(HEADER_PATH)
    out_dir.mkdir(parents=True, exist_ok=True)

    try:
        for node_id in nodes:
            gh_id = gh_for_node(node_id)
            settings["gh_id"] = str(gh_id)
            settings["node_id"] = str(node_id)
            settings["firmware_version"] = args.version
            save_config(CONFIG_PATH, config)

            print(f"[BUILD] GH={gh_id} NODE={node_id} VERSION={args.version}")
            run_build(pio, args.env)

            source = REPO_ROOT / ".pio" / "build" / args.env / "firmware.bin"
            if not source.exists():
                raise FileNotFoundError(f"Expected build output not found: {source}")

            target = out_dir / f"node{node_id}.bin"
            shutil.copy2(source, target)
            try:
                target_display = target.relative_to(REPO_ROOT)
            except ValueError:
                target_display = target
            print(f"  -> copied to {target_display}")
    finally:
        restore_file(CONFIG_PATH, original_config)
        restore_file(HEADER_PATH, original_header)

    print("[DONE] All builds complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
