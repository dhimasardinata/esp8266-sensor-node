#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVICE_NAME="node-medini-watchers.service"
UNIT_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user"
UNIT_PATH="${UNIT_DIR}/${SERVICE_NAME}"
NODE_BIN="$(command -v node || true)"
NPM_BIN="$(command -v npm || true)"

if [[ -z "${NODE_BIN}" ]]; then
  echo "node binary not found in PATH" >&2
  exit 1
fi

if [[ ! -d "${REPO_ROOT}/node_modules/chokidar" ]]; then
  if [[ -z "${NPM_BIN}" ]]; then
    echo "npm binary not found in PATH and chokidar is not installed" >&2
    exit 1
  fi

  echo "[watchers] Installing npm dependencies..."
  "${NPM_BIN}" install --prefix "${REPO_ROOT}" --no-fund --no-audit
fi

mkdir -p "${UNIT_DIR}"

cat >"${UNIT_PATH}" <<EOF
[Unit]
Description=node-medini repo watcher service
ConditionPathExists=${REPO_ROOT}/tools/watch/repo-watchers.js

[Service]
Type=simple
WorkingDirectory=${REPO_ROOT}
ExecStart=${NODE_BIN} ${REPO_ROOT}/tools/watch/repo-watchers.js
Restart=always
RestartSec=2
Environment=NODE_ENV=development

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now "${SERVICE_NAME}"
systemctl --user restart "${SERVICE_NAME}"
systemctl --user --no-pager --full status "${SERVICE_NAME}"
