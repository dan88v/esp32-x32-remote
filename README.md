# ESP32 X32 Remote

A compact wired remote controller for **Behringer X32** and **Midas M32** digital mixers. It uses an ESP32, an ENC28J60 Ethernet module, a rotary encoder with push button, and a 16×2 I²C LCD to control and monitor a selected mixer channel over OSC/UDP.

## Features

- Controls all 72 X32/M32 strip indexes:
  - 32 input channels
  - 8 auxiliary inputs
  - 8 FX returns
  - 16 buses
  - 6 matrices
  - Main LR and Mono/Center
- Rotary control for the selected strip's fader.
- Long press to toggle the selected strip on/off (mute).
- Displays channel name, fader value, mute state, and meter activity.
- Receives live mixer feedback through `/xremote`.
- Uses X32/M32 meter subscriptions for channel and Main LR metering.
- Stores controller IP, mixer IP, and selected channel in ESP32 NVS.
- Provides a temporary four-digit control lock.
- Uses non-blocking runtime logic for responsive OSC and input handling.

## Hardware

| Quantity | Component |
| --- | --- |
| 1 | ESP32 development board |
| 1 | ENC28J60 Ethernet module |
| 1 | 16×2 LCD with I²C backpack, normally at `0x27` |
| 1 | Rotary encoder with integrated push button |
| 1 | Suitable power supply and wiring |

## Default pin configuration

| Function | ESP32 pin |
| --- | ---: |
| Rotary encoder A | GPIO 27 |
| Rotary encoder B | GPIO 26 |
| Encoder push button | GPIO 25 |
| ENC28J60 chip select | GPIO 5 |

The sketch uses the ESP32 board package's default SPI and I²C pins. On many ESP32 DevKit boards these are:

- SPI: SCK 18, MISO 19, MOSI 23
- I²C: SDA 21, SCL 22

Verify the pinout of your exact board before wiring. All configurable hardware and timing values are grouped in [`Config.h`](esp32-x32-remote/Config.h).

## Network defaults

| Device | Address |
| --- | --- |
| ESP32 controller | `10.0.1.2` |
| X32/M32 mixer | `10.0.1.1` |
| OSC UDP port | `10023` |

The controller and mixer must be in the same IPv4 subnet. Both IP addresses can be changed from the device menu and are retained after reboot.

## Required Arduino libraries

Install the ESP32 board support package and these libraries through Arduino IDE's Library Manager or from their upstream repositories:

- `LiquidCrystal_I2C`
- `EthernetENC`
- `OSC` by CNMAT
- `Button2`
- `ESPRotary`

`Preferences`, `SPI`, and the base Arduino APIs are supplied by the ESP32 Arduino core.

## Build and upload

1. Clone or download this repository.
2. Open `esp32-x32-remote/esp32-x32-remote.ino` in Arduino IDE.
3. Select the correct ESP32 board and serial port.
4. Install the required libraries listed above.
5. Review `Config.h`, especially the LCD address, encoder pins, Ethernet CS pin, and default IP addresses.
6. Compile and upload the sketch.
7. Connect the controller and mixer to the same Ethernet network.

## Controls

### Home screen

- Rotate the encoder to change the selected strip's fader.
- Long-press the encoder button to toggle the strip on/off.
- Short-press the encoder button to open the main menu.

The firmware waits for the current fader or mute state before changing it. This prevents a control action from jumping to an uninitialized value immediately after startup or channel selection.

### Main menu

- **Select Channel** — chooses the controlled X32/M32 strip.
- **Set Local IP** — changes and saves the ESP32 address.
- **Set Mixer IP** — changes and saves the mixer address.
- **Lock Controls** — sets a four-digit PIN and locks fader/mute operation until the PIN is entered again.

Rotate to change a value, short-press to confirm it, and long-press to cancel a menu and return home.

The control lock is intentionally session-only: rebooting the ESP32 clears it. Network settings and the selected channel remain saved.

## Display states

The LCD reports the most important connection states:

- `No cable` — no Ethernet link is detected.
- `Connecting to mixer...` — Ethernet is available but no valid OSC response has arrived yet.
- `Mixer offline` — subscription renewals have not received a response.
- `CONTROLS LOCKED` — fader and mute controls are disabled until the PIN is entered.

## Project structure

```text
esp32-x32-remote/
├── esp32-x32-remote.ino   # Arduino entry point
├── Config.h               # Hardware, network, and timing configuration
├── ChannelCatalog.h       # Channel metadata declarations
├── ChannelCatalog.cpp     # X32/M32 strip catalog
├── X32Remote.h            # Controller interface and state
└── X32Remote.cpp          # UI, Ethernet, OSC, persistence, and controls
```

## Known limitations

- Ethernet only; Wi-Fi is not currently implemented.
- The firmware uses a fixed static IPv4 configuration and does not provide DHCP or automatic mixer discovery.
- The 16×2 LCD limits how much channel metadata can be shown.
- ENC28J60 and LCD modules vary; wiring, power requirements, and I²C address may differ.
- Changes should be verified on physical hardware before use in a live production environment.

## Roadmap ideas

- Optional Wi-Fi transport.
- DHCP and mixer discovery.
- Configurable fader step and encoder direction.
- Additional channel parameters and user-assignable shortcuts.
- Reproducible automated firmware builds.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
