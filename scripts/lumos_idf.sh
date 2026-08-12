#!/usr/bin/env bash
# Build / flash LumosOS for esp32 or esp32s3 from one source tree.
#
# Usage:
#   ./scripts/lumos_idf.sh esp32 build
#   ./scripts/lumos_idf.sh esp32s3 build
#   ./scripts/lumos_idf.sh esp32 flash
#   ./scripts/lumos_idf.sh auto flash          # detect chip on PORT
#   ./scripts/lumos_idf.sh auto build-flash
#
# Env:
#   PORT   serial port (optional; passed to idf.py / esptool)
#   IDF    path to export.sh (default: $HOME/esp/esp-idf/export.sh)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

TARGET="${1:-}"
ACTION="${2:-build}"
PORT="${PORT:-}"
IDF_EXPORT="${IDF:-$HOME/esp/esp-idf/export.sh}"

usage() {
  sed -n '2,14p' "$0" | sed 's/^# \?//'
  exit 1
}

[[ -n "$TARGET" ]] || usage

if [[ ! -f "$IDF_EXPORT" ]]; then
  echo "ESP-IDF export not found: $IDF_EXPORT" >&2
  echo "Set IDF=/path/to/esp-idf/export.sh" >&2
  exit 1
fi
# shellcheck disable=SC1090
source "$IDF_EXPORT" >/dev/null

detect_target() {
  local out
  if [[ -n "$PORT" ]]; then
    out="$(esptool.py -p "$PORT" chip_id 2>&1 || true)"
  else
    out="$(esptool.py chip_id 2>&1 || true)"
  fi
  if echo "$out" | grep -qi 'ESP32-S3'; then
    echo esp32s3
  elif echo "$out" | grep -qiE 'Chip is ESP32([^A-Za-z-]|$)'; then
    echo esp32
  else
    echo "$out" >&2
    echo "Could not detect ESP32 / ESP32-S3 on the serial port." >&2
    echo "Pass an explicit target or set PORT=/dev/cu.xxx" >&2
    exit 1
  fi
}

if [[ "$TARGET" == "auto" ]]; then
  TARGET="$(detect_target)"
  echo "Detected target: $TARGET"
fi

case "$TARGET" in
  esp32|esp32s3) ;;
  *)
    echo "Unsupported target: $TARGET (use esp32, esp32s3, or auto)" >&2
    exit 1
    ;;
esac

BUILD_DIR="build-${TARGET}"
SDKCONFIG="sdkconfig.${TARGET}"

idf_cmd() {
  if [[ -n "$PORT" ]]; then
    idf.py -B "$BUILD_DIR" -D SDKCONFIG="$SDKCONFIG" -p "$PORT" "$@"
  else
    idf.py -B "$BUILD_DIR" -D SDKCONFIG="$SDKCONFIG" "$@"
  fi
}

# Ensure this build dir is configured for the requested target.
need_set_target=0
if [[ ! -f "$SDKCONFIG" ]]; then
  need_set_target=1
elif ! grep -q "CONFIG_IDF_TARGET=\"${TARGET}\"" "$SDKCONFIG" 2>/dev/null; then
  need_set_target=1
fi

if [[ "$need_set_target" -eq 1 ]]; then
  echo "Configuring $TARGET (SDKCONFIG=$SDKCONFIG, build=$BUILD_DIR)…"
  idf_cmd set-target "$TARGET"
fi

case "$ACTION" in
  build)
    idf_cmd build
    ;;
  flash)
    idf_cmd flash
    ;;
  build-flash|flash-build)
    idf_cmd build flash
    ;;
  monitor)
    idf_cmd monitor
    ;;
  menuconfig)
    idf_cmd menuconfig
    ;;
  *)
    echo "Unknown action: $ACTION (build|flash|build-flash|monitor|menuconfig)" >&2
    exit 1
    ;;
esac

echo "Done: target=$TARGET action=$ACTION sdkconfig=$SDKCONFIG build=$BUILD_DIR"
