#!/usr/bin/env bash
# Build / flash LumosOS (esp32 / esp32s3).
# Doorbell TX is forwarded to sibling ../esp32-hotkeys.
#
# Usage:
#   ./scripts/lumos_idf.sh esp32 build
#   ./scripts/lumos_idf.sh esp32s3 build
#   ./scripts/lumos_idf.sh esp32 flash
#   ./scripts/lumos_idf.sh auto flash          # detect chip on PORT (LumosOS)
#   ./scripts/lumos_idf.sh auto build-flash
#   ./scripts/lumos_idf.sh doorbell-tx build
#   ./scripts/lumos_idf.sh esp32 doorbell-tx flash
#   ./scripts/lumos_idf.sh doorbell-tx-8266 build
#   ./scripts/lumos_idf.sh esp8266 doorbell-tx flash
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
HOTKEYS_ROOT="$ROOT/../esp32-hotkeys"

usage() {
  sed -n '2,18p' "$0" | sed 's/^# \?//'
  exit 1
}

[[ -n "$ARG1" ]] || usage

# Doorbell transmitter builds moved to sibling `esp32-hotkeys` repo.
# Supported forms:
#   ./scripts/lumos_idf.sh doorbell-tx build|flash
#   ./scripts/lumos_idf.sh doorbell-tx-8266 build|flash
#   ./scripts/lumos_idf.sh esp32 doorbell-tx build|flash
#   ./scripts/lumos_idf.sh esp8266 doorbell-tx build|flash
if [[ "$ARG1" == "doorbell-tx" ]]; then
  ACTION="${ARG2:-build}"
  exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx "$ACTION"
fi
if [[ "$ARG1" == "doorbell-tx-8266" ]]; then
  ACTION="${ARG2:-build}"
  exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx-8266 "$ACTION"
fi
if [[ "$ARG1" == "esp8266" && "$ARG2" == "doorbell-tx" ]]; then
  ACTION="${ARG3:-build}"
  exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx-8266 "$ACTION"
fi
if [[ "$ARG2" == "doorbell-tx" ]]; then
  ACTION="${ARG3:-build}"
  # Treat `esp8266 doorbell-tx` as the PlatformIO target.
  if [[ "$ARG1" == "esp8266" ]]; then
    exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx-8266 "$ACTION"
  fi
  exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx "$ACTION"
fi
if [[ "$ARG2" == "doorbell-tx-8266" ]]; then
  ACTION="${ARG3:-build}"
  exec bash "$HOTKEYS_ROOT/scripts/hotkeys_idf.sh" doorbell-tx-8266 "$ACTION"
fi

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
    echo "Unsupported target: $TARGET (use esp32, esp32s3, auto, doorbell-tx, or doorbell-tx-8266)" >&2
    exit 1
    ;;
esac

if [[ "$PRODUCT" == "doorbell-tx" ]]; then
  BUILD_DIR="$HOTKEYS_ROOT/build-doorbell-tx"
  SDKCONFIG="$HOTKEYS_ROOT/sdkconfig.doorbell-tx"
  PROJECT_DIR="$HOTKEYS_ROOT/doorbell_tx"
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
