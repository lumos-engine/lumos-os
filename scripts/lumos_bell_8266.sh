#!/usr/bin/env bash
# Build / flash the doorbell transmitter for ESP8266 (Arduino / PlatformIO).
#
# Usage:
#   ./scripts/lumos_bell_8266.sh build
#   ./scripts/lumos_bell_8266.sh flash
#   ./scripts/lumos_bell_8266.sh build-flash
#   ./scripts/lumos_bell_8266.sh monitor
#
# Env:
#   PORT   serial port (optional)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ACTION="${1:-build}"
PORT="${PORT:-}"

# Doorbell transmitter builds moved to sibling `esp32-hotkeys` repo.
HOTKEYS_ROOT="$ROOT/../esp32-hotkeys"
if [[ ! -d "$HOTKEYS_ROOT" ]]; then
  echo "esp32-hotkeys not found at $HOTKEYS_ROOT" >&2
  exit 1
fi

exec bash "$HOTKEYS_ROOT/scripts/hotkeys_bell_8266.sh" "$ACTION"
