# LumosOS

Premium, extensible firmware for ESP32-based addressable LED controllers.

LumosOS is an LED operating system. HyperHDR is one plugin among many.

## Hardware (v0.3)

| Item          | Value                               |
| ------------- | ----------------------------------- |
| MCU           | ESP32-WROOM-32                      |
| LEDs          | WS2815 (primary), SK6812 RGB / RGBW |
| Default count | 140 (44 / 26 / 44 / 26 layout)      |
| Default GPIO  | 16                                  |
| Power         | Match your strip (e.g. 12V WS2815)  |

## Features (v0.3)

- Plugin framework with **capability metadata** for Studio / Matter UI generation
- Built-ins: Off, Static, Bias White, Rainbow, HyperHDR, Aurora, Fire, Twinkle
- **ColorProcessor** pipeline: gamma → brightness → RGB→RGBW → power limit → driver
- WS2815 / SK6812 RMT driver with RGBW (`led_strip_set_pixel_rgbw`)
- Configurable perimeter layout (top / right / bottom / left)
- Smart startup + configurable HyperHDR fallback
- WiFi STA + AP captive portal, static IP
- **DHCP hostname** defaults to `LumosOS` (routers no longer show `espressif`)
- **Multi-device discovery**: mDNS `_lumosos._tcp` + `GET /api/v1/neighbors`
- **Matter** extended color light (OnOff / LevelControl / ColorControl) → Off / Static / Bias + brightness
- REST + WebSocket APIs (`api: "0.3"`)
- Browser OTA + recovery web UI (neighbors list, Matter pairing / factory reset)
- HyperHDR via **DDP** (UDP 4048) / Hyperk (`/json` + `/json/state`)

Deferred to **v0.4**: Music Reactive.

## Matter commissioning

1. Connect the board to Wi‑Fi via the recovery UI (Matter does not replace LumosOS Wi‑Fi setup).
2. Open the recovery UI → **Matter** section for the manual pairing code and QR payload string.
3. Add the device in Apple Home / Google Home / Alexa using that code (BLE commissioning).
4. Matter On/Off maps to the Off / Static (HSV) / Bias (color temperature) plugins; Level maps to device brightness.
5. **Factory reset Matter** clears fabrics in NVS and reboots (Wi‑Fi credentials are kept). A full NVS erase also clears Matter.

Default test passcode / discriminator follow ESP-Matter defaults (`20202021` / `3840`) unless factory data is provisioned.

## Multi-device discovery

- Each board advertises `_lumosos._tcp` with TXT: `version`, `api`, `leds`, `chipset`, `path`.
- `GET /api/v1/neighbors` returns cached browse results (self excluded).
- Recovery UI lists nearby boards (name + IP + link). No mesh sync in v0.3.

## Build

Requires [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/). Matter pulls `espressif/esp_matter` via the component manager (first build downloads a large dependency tree).

```bash
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
```

Watch app size vs the `0x1e0000` OTA slot if you enable additional Matter features.

## First boot

1. Device opens AP `LumosOS-Setup` if no WiFi credentials are stored.
2. Captive portal configures STA WiFi.
3. Open `http://lumosos.local` (mDNS label) or the device IP for recovery UI. Router DHCP client list should show **LumosOS**.
4. Set LED count + layout to match HyperHDR (e.g. 140 = 44+26+44+26).
5. Point HyperHDR at the device using the **Hyperk** or **DDP** LED driver (port 4048).

## Host unit tests

```bash
cmake -S host_tests -B host_tests/build
cmake --build host_tests/build
ctest --test-dir host_tests/build --output-on-failure
```

## Architecture

Plugins render RGB into a framebuffer. `ColorProcessor` + `Renderer` own presentation (including RGBW). Plugins never touch the LED driver. `lumos_matter` bridges Matter clusters into PluginManager + Preferences.

See `components/` for modular services (`lumos_core`, `lumos_plugin`, `lumos_renderer`, `lumos_matter`, …).

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
