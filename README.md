# LumosOS

Premium, extensible firmware for ESP32-based addressable LED controllers.

LumosOS 0.1 is an LED operating system. HyperHDR is one plugin among many.

## Hardware (v0.1)

| Item          | Value                    |
| ------------- | ------------------------ |
| MCU           | ESP32-WROOM-32           |
| LEDs          | WS2815 (primary)         |
| Default count | 150                      |
| Default GPIO  | 16                       |
| Power         | 12V, inject at both ends |

## Features (v0.1)

- Plugin framework (Off, Static, Bias White, Rainbow, HyperHDR)
- WS2815 RMT driver with gamma, brightness, and power limiting
- Smart startup + configurable HyperHDR fallback
- WiFi STA + AP captive portal
- REST + WebSocket APIs with plugin metadata discovery
- Browser OTA + lightweight recovery web UI
- HyperHDR via **DDP** (UDP 4048) — HyperHDR’s recommended wireless path

## Build

Requires [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/).

```bash
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
```

## First boot

1. Device opens AP `LumosOS-Setup` if no WiFi credentials are stored.
2. Captive portal configures STA WiFi.
3. Open `http://lumosos.local` (or device IP) for recovery UI.
4. Point HyperHDR at the device using the **Hyperk** or **DDP** LED driver (port 4048).

## Host unit tests

```bash
cmake -S host_tests -B host_tests/build
cmake --build host_tests/build
ctest --test-dir host_tests/build --output-on-failure
```

## Architecture

Plugins render into a framebuffer. The renderer owns LEDs. Plugins never touch hardware directly.

See `components/` for modular services (`lumos_core`, `lumos_plugin`, `lumos_renderer`, …).

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
