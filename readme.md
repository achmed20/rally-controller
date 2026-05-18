# Rally Controller

A DIY BLE gamepad controller for the DMD2 tablet, designed to mount on a 22mm handlebar. About 22.5mm wide. Built because proper rally controllers cost a fortune.

It appears to the host device as **"Rally Remote"** and simulates a standard gamepad over Bluetooth LE.

![Rally Controller](3d%20models/image.png)
![Rally Controller mounted](3d%20models/image2.png)

---

## Parts List

| Part | Qty |
|------|-----|
| PBS 33B M12 momentary button | 3 |
| MTS-123 2-way rocker switch | 1 |
| M3 x 6mm screws | 4 |
| M4 x 15mm screws | 2 |
| M4 nuts | 2 |
| ~50cm 6-core cable (as thin as possible) | 1 |
| Small zip tie | 1 |
| ESP32 with BLE support | 1 |

---

## 3D Printed Parts

STL files and the full 3MF project are in the [`3d models/`](3d%20models/) folder.

| File | Description |
|------|-------------|
| `remote base.stl` | Main housing |
| `remote backplate.stl` | Rear cover |
| `clamp.stl` | Handlebar clamp |
| `rally_remote_full.3mf` | Full assembly (all parts) |

---

## Assembly

1. Press-fit the M4 nuts into the backplate. If they're loose, secure with a drop of glue.
2. Install the buttons and tighten with their included nuts. Start with the top button, then the 2-way rocker, then the remaining buttons. Orient the terminals toward the outer walls to make soldering easier.
3. Use loctite or superglue to lock the nuts in place.
4. Strip a short piece of wire and solder a GND backbone — one terminal of each button connected to ground. This minimises the amount of individual wires you need.
5. Feed the 6-core cable through the housing and solder the remaining signal wires.
6. Secure the cable inside the case with a zip tie looped around the 6-core cable so it can't be pulled through.
7. Fit the backplate and secure with 4× M3×6mm screws.

---

## Pin Layout

All pins use the ESP32's internal pull-ups. Buttons connect the pin to GND when pressed.

| GPIO | Function |
|------|----------|
| 25 | Forward (rocker) |
| 26 | Backwards (rocker) |
| 32 | Button 1 |
| 33 | Button 2 |
| 27 | Button 3 (multi-press) |

To use a different ESP32 board, change the `#define PIN_*` values at the top of [`src/main.cpp`](src/main.cpp).

---

## Button Mapping

| Input | Gamepad output |
|-------|---------------|
| Forward rocker | Hat right |
| Backwards rocker | Hat left |
| Button 1 | A (Button 1) |
| Button 2 | B (Button 2) |
| Button 3 — 1× press | X (Button 3) |
| Button 3 — 2× press | Y (Button 4) |
| Button 3 — 3× press | LB (Button 5) |
| Button 3 — long press | RB (Button 6) |

Multi-press window is 400 ms. Long press threshold is 2000 ms.

> **Pairing mode:** Hold Button 3 for 15 seconds to restart BLE advertising and make the device discoverable again.

---

## Software / Build

### Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32 board support (`espressif32` platform)
- Library: [`lemmingdev/ESP32-BLE-Gamepad`](https://github.com/lemmingDEV/ESP32-BLE-Gamepad) — installed automatically by PlatformIO

### Build & Flash

```bash
# Build
pio run

# Build and upload
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor
```

Or use the PlatformIO sidebar in VS Code (Build / Upload / Monitor buttons).

The [`platformio.ini`](platformio.ini) targets the `esp32dev` board. Change the `board` value there if you're using a different variant (e.g. `lolin32`, `esp32-s3-devkitc-1`).

### First Run / Pairing

1. Flash the firmware.
2. If the device doesn't show up on your phone/tablet, reboot the controller after flashing.
3. Pair it like any Bluetooth gamepad — it will appear as **"Rally Remote"**.
4. If you need to re-pair, hold Button 3 for 15 seconds to re-enter advertising mode.
