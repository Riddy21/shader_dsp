#!/usr/bin/env bash
set -euo pipefail

# This script ensures the container uses /workspace/.cursor_server for Cursor settings
# It will copy from /host_cursor_server (mounted from the host) if available and if
# /workspace/.cursor_server is empty, then it will symlink ~/.cursor_server to it.

TARGET_DIR="/workspace/.cursor_server"
HOST_DIR="/host_cursor_server"
HOME_DIR="/root"

mkdir -p "${TARGET_DIR}"

# If host config is mounted and target is empty, copy it over once
if [ -d "${HOST_DIR}" ] && [ -z "$(ls -A "${TARGET_DIR}" 2>/dev/null)" ]; then
  echo "Syncing host Cursor settings from ${HOST_DIR} to ${TARGET_DIR}..."
  cp -a "${HOST_DIR}/." "${TARGET_DIR}/"
fi

# Replace any existing non-symlink ~/.cursor_server with a symlink to the target
if [ -e "${HOME_DIR}/.cursor_server" ] && [ ! -L "${HOME_DIR}/.cursor_server" ]; then
  rm -rf "${HOME_DIR}/.cursor_server"
fi

ln -sfn "${TARGET_DIR}" "${HOME_DIR}/.cursor_server"

echo "Cursor settings are available at ${HOME_DIR}/.cursor_server -> ${TARGET_DIR}"

