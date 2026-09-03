# LumosOS

Premium, extensible firmware for ESP32-based addressable LED controllers.

LumosOS is an LED operating system. HyperHDR is one plugin among many.

## Hardware (v0.3)

| Item          | Value                                      |
| ------------- | ------------------------------------------ |
| MCU           | ESP32 or ESP32-S3 (one source, per-target build) |
| LEDs          | WS2815 (primary), SK6812 RGB / RGBW        |
| Default count | 140 (44 / 26 / 44 / 26 layout)             |
| Default GPIO  | 16 (LED), 17 (doorbell relay)              |
| Power         | Match your strip (e.g. 12V WS2815)         |

## Features (v0.3)

- Plugin framework with **capability metadata** for Studio UI generation
- Built-ins: Off, Static, Bias White, Rainbow, HyperHDR, Aurora, Fire, Twinkle
- **ColorProcessor** pipeline: gamma → brightness → RGB→RGBW → power limit → driver
- WS2815 / SK6812 RMT driver with RGBW (`led_strip_set_pixel_rgbw`)
- Configurable perimeter layout (top / right / bottom / left)
- Smart startup; HyperHDR / Lumos Vision hold black on stream silence (same plugin)
- WiFi STA + AP captive portal, static IP
- **DHCP hostname** defaults to `LumosOS` (routers no longer show `espressif`)
- **Multi-device discovery**: mDNS `_lumosos._tcp` + `GET /api/v1/neighbors`
- REST + WebSocket APIs (`api: "0.3"`)
- Browser OTA + recovery web UI (neighbors list)
- **Config backup/restore**: `GET/POST /api/v1/config` JSON clone (strip, balance, plugins, optional Wi‑Fi)
- **LED calibration / edges**: perimeter start + CW/CCW orientation, ignore ends/corners, Calibration plugin + tap-to-ignore
- HyperHDR via **DDP** (UDP 4048) / Hyperk (`/json` + `/json/state`)

Deferred: **Matter** (ESP32-WROOM RAM too tight with the full stack; `lumos_matter` source kept), Music Reactive → v0.4+.

## Multi-device discovery

- Each board advertises `_lumosos._tcp` with TXT: `version`, `api`, `leds`, `chipset`, `path`.
- `GET /api/v1/neighbors` returns cached browse results (self excluded).
- Recovery UI lists nearby boards (name + IP + link). No mesh sync in v0.3.

## Build

Requires [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/).

One source tree; **separate builds** per chip (binaries are not interchangeable):

```bash
. $HOME/esp/esp-idf/export.sh

# Classic ESP32
./scripts/lumos_idf.sh esp32 build
./scripts/lumos_idf.sh esp32 flash          # optional: PORT=/dev/cu.xxx

# ESP32-S3
./scripts/lumos_idf.sh esp32s3 build
./scripts/lumos_idf.sh esp32s3 flash

# Auto-detect the connected chip, then build+flash
PORT=/dev/cu.usbserial-0001 ./scripts/lumos_idf.sh auto build-flash

# Doorbell transmitters live in the sibling esp32-hotkeys repo
../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx build
../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx-8266 build
```

Each target keeps its own `sdkconfig.<target>` and `build-<target>/` directory. Shared defaults live in `sdkconfig.defaults`; chip-specific overrides in `sdkconfig.defaults.esp32` / `sdkconfig.defaults.esp32s3`. Pin safety helpers are in `components/lumos_core/include/lumos/core/board_pins.hpp`.

Manual equivalent:

```bash
idf.py -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 set-target esp32 build
idf.py -B build-esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3 build
```

## First boot

1. Device opens AP `LumosOS-Setup` if no WiFi credentials are stored.
2. Captive portal configures STA WiFi.
3. Open `http://lumosos.local` (mDNS label) or the device IP for recovery UI. Router DHCP client list should show **LumosOS**.
4. Set LED count + layout to match HyperHDR (e.g. 140 = 44+26+44+26).
5. Point HyperHDR at the device using the **Hyperk** or **DDP** LED driver (port 4048).

## Doorbell transmitters (ESP #1)

Separate slim firmware in the sibling [`esp32-hotkeys`](../esp32-hotkeys) repo. Same
ESP-NOW wire format on classic ESP32 and ESP8266; LumosOS stays the receiver.

```bash
# Classic ESP32 (ESP-IDF)
../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx build
PORT=/dev/cu.usbserial-0001 ../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx flash

# ESP8266 (PlatformIO / Arduino)
../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx-8266 build
PORT=/dev/cu.usbserial-0001 ../esp32-hotkeys/scripts/hotkeys_idf.sh doorbell-tx-8266 flash
```

1. Join AP `LumosOS-Bell`, open `http://192.168.4.1/`
2. Connect the bell to the same router as the LED board (or pair on channel)
3. On LumosOS `/doorbell` → Start pairing, then Find nearby on the bell
4. Optocoupler default: **GPIO 4**, active-LOW (collector to GPIO, emitter to GND)

ESP-NOW does not need a router; both boards must be on the **same channel**.
Config JSON is interchangeable between ESP32 and ESP8266 bells (ESP8266 only
accepts interrupt-safe GPIOs: 4, 5, 12, 13, 14 — invalid pins fall back to 4).

## Host unit tests

```bash
cmake -S host_tests -B host_tests/build
cmake --build host_tests/build
ctest --test-dir host_tests/build --output-on-failure
```

## Architecture

Plugins render RGB into a framebuffer. `ColorProcessor` + `Renderer` own presentation (including RGBW). Plugins never touch the LED driver.

See `components/` for modular services (`lumos_core`, `lumos_plugin`, `lumos_renderer`, …). Matter sources remain under `components/lumos_matter/` but are not linked.

## License

LumosOS — including **all past and present commits** in this repository — is
licensed under the [GNU General Public License v3.0](LICENSE), with
[Additional Terms](NOTICE) under GPL §7.

In short:

- Derivative works and redistributed copies must remain open source under GPL-3.0.
- Products built with LumosOS must give clear front-page credit that LumosOS was used to build them (see `NOTICE`).

```
Copyright (C) 2026 Shivansh Tyagi
```
