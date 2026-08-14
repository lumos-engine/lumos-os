#!/usr/bin/env bash
# Build / flash LumosOS (esp32 / esp32s3) or the doorbell transmitter (esp32 only).
#
# Usage:
#   ./scripts/lumos_idf.sh esp32 build
#   ./scripts/lumos_idf.sh esp32s3 build
#   ./scripts/lumos_idf.sh esp32 flash
#   ./scripts/lumos_idf.sh auto flash          # detect chip on PORT (LumosOS)
#   ./scripts/lumos_idf.sh auto build-flash
#   ./scripts/lumos_idf.sh doorbell-tx build
#   ./scripts/lumos_idf.sh esp32 doorbell-tx flash
#
# Env:
#   PORT   serial port (optional; passed to idf.py / esptool)
#   IDF    path to export.sh (default: $HOME/esp/esp-idf/export.sh)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ARG1="${1:-}"
ARG2="${2:-}"
ARG3="${3:-}"
PORT="${PORT:-}"
IDF_EXPORT="${IDF:-$HOME/esp/esp-idf/export.sh}"

usage() {
  sed -n '2,16p' "$0" | sed 's/^# \?//'
  exit 1
}

[[ -n "$ARG1" ]] || usage

if [[ ! -f "$IDF_EXPORT" ]]; then
  echo "ESP-IDF export not found: $IDF_EXPORT" >&2
  echo "Set IDF=/path/to/esp-idf/export.sh" >&2
  exit 1
fi
# shellcheck disable=SC1090
source "$IDF_EXPORT" >/dev/null

PRODUCT="lumos"
TARGET=""
ACTION="build"

if [[ "$ARG1" == "doorbell-tx" ]]; then
  PRODUCT="doorbell-tx"
  TARGET="esp32"
  ACTION="${ARG2:-build}"
elif [[ "$ARG2" == "doorbell-tx" ]]; then
  PRODUCT="doorbell-tx"
  TARGET="$ARG1"
  ACTION="${ARG3:-build}"
else
  TARGET="$ARG1"
  ACTION="${ARG2:-build}"
fi

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

if [[ "$PRODUCT" == "doorbell-tx" && "$TARGET" != "esp32" ]]; then
  echo "doorbell-tx is classic ESP32 only (got $TARGET)" >&2
  exit 1
fi

case "$TARGET" in
  esp32|esp32s3) ;;
  *)
    echo "Unsupported target: $TARGET (use esp32, esp32s3, auto, or doorbell-tx)" >&2
    exit 1
    ;;
esac

if [[ "$PRODUCT" == "doorbell-tx" ]]; then
  BUILD_DIR="$ROOT/build-doorbell-tx"
  SDKCONFIG="$ROOT/sdkconfig.doorbell-tx"
  PROJECT_DIR="$ROOT/doorbell_tx"
else
  BUILD_DIR="$ROOT/build-${TARGET}"
  SDKCONFIG="$ROOT/sdkconfig.${TARGET}"
  PROJECT_DIR="$ROOT"
fi

idf_cmd() {
  local extra=()
  extra+=(-C "$PROJECT_DIR" -B "$BUILD_DIR" -D "SDKCONFIG=$SDKCONFIG")
  if [[ -n "$PORT" ]]; then
    extra+=(-p "$PORT")
  fi
  idf.py "${extra[@]}" "$@"
}

need_set_target=0
if [[ ! -f "$SDKCONFIG" ]]; then
  need_set_target=1
elif ! grep -q "CONFIG_IDF_TARGET=\"${TARGET}\"" "$SDKCONFIG" 2>/dev/null; then
  need_set_target=1
fi

if [[ "$need_set_target" -eq 1 ]]; then
  echo "Configuring $PRODUCT $TARGET (SDKCONFIG=$SDKCONFIG, build=$BUILD_DIR)…"
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

echo "Done: product=$PRODUCT target=$TARGET action=$ACTION sdkconfig=$SDKCONFIG build=$BUILD_DIR"
