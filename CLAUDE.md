# CLAUDE.md — Wio Terminal Workbench

## Project overview

A personal toolkit for the Seeed Wio Terminal. Each screen is a `.cpp` file with a single blocking function; the joystick-navigated menu wires them together. Currently ships with a Home sensor dashboard, a Claude Usage screen, a Sys Stats screen, and a brightness settings screen. New screens slot in without touching anything outside `homeScreen.cpp` and `globals.h`.

Built with PlatformIO (`atmelsam` platform, `arduino` framework, `seeed_wio_terminal` board).

## Hardware reference

**MCU:** Microchip ATSAMD51P19 — ARM Cortex-M4F @ 120MHz (overclockable to 200MHz), 512KB flash, 192KB RAM, 4MB external flash.

**Display:** 2.4" ILI9341 LCD, 320×240px. Driven via `Seeed_Arduino_LCD` (a pre-configured TFT_eSPI fork). Always use rotation 3 for landscape.

**Wireless co-processor:** RTL8720DN — Wi-Fi 802.11 a/b/g/n (2.4GHz + 5GHz) and BLE 5.0. Runs independently from the SAMD51; communicates over RPC. BLE stack requires `Seeed_Arduino_rpcUnified` + `Seeed_Arduino_rpcBLE`.

**Onboard peripherals available for future screens:**

| Peripheral | Notes |
|---|---|
| LIS3DHTR accelerometer | I2C — motion, tilt, tap detection |
| Microphone | Analog — 1.0–10V, -42dB |
| Speaker | PWM — ≥78dB @10cm |
| Light sensor | Analog — 400–1050nm |
| IR emitter | 940nm |
| microSD slot | SPI — up to 16GB; needs `Seeed_Arduino_FS` |

**Interfaces:** Grove ×2, 40-pin RPi-compatible header, 20-pin FPC, USB-C (power + OTG host/client).

## Bootloader & recovery

- **Normal reset:** flip the power/reset switch once.
- **Bootloader mode:** double-tap the reset switch quickly — the blue LED will pulse slowly. Use this if the device stops being detected after a bad flash (e.g. USB mishandled in firmware).
- **SWD debug pads** (on PCB back): SWCLK, SWDIO, SWO, RST, GND, 3V3 for the SAMD51; separate pads for the RTL8720DN.

## Build

```
pio run                   # build only
pio run --target upload   # build + upload over USB (bossac)
```

## Project structure

```
src/
  main.cpp          — Global definitions, setup(), loop(), top-level screen dispatch
  homeScreen.cpp    — Menu render + joystick navigation; register new screens here
  backlight.cpp     — Brightness sub-screen (setBrightness)
  battery.cpp       — BQ27441-G1A I²C driver + drawBatteryStatus() overlay
  bluetooth.cpp     — BLE GATT peripheral (WT-001); deferred init, on-demand advertising
  claudeUsage.cpp   — Claude Usage screen: JSON parser, serial reader, usage display
  sysStats.cpp      — Sys Stats screen: arc gauges for CPU/RAM/GPU/network via sysstat_sender.py
include/
  globals.h         — extern declarations and function prototypes for all .cpp files
  lcd_backlight.hpp — SAMD51 TC0 PWM backlight driver (Seeed original, Boost licence)
tools/
  claude_sender.py  — Feeds Claude usage data over USB serial (default) or BLE (--ble flag)
  sysstat_sender.py — Feeds PC system stats (CPU/RAM/GPU/net) over USB serial or BLE (--ble flag)
  bitmap-converter/ — PySide6 GUI for converting images to Wio Terminal bitmap format
```

## Adding a new screen

### 1. Create `src/myScreen.cpp`

```cpp
#include <Arduino.h>
#include "globals.h"

void myScreen()
{
    // Wait for the joystick press that launched this screen to be released.
    while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }

    tft.fillScreen(TFT_BLACK);
    // ... draw your screen ...
    drawBatteryStatus(TFT_BLACK);  // optional corner overlay

    while (true)
    {
        // ... handle input ...
        if (digitalRead(WIO_KEY_C) == LOW)
        {
            while (digitalRead(WIO_KEY_C) == LOW) { delay(10); }
            delay(50);
            return;  // back to menu
        }
        delay(20);
    }
}
```

### 2. Declare it in `include/globals.h`

```cpp
// myScreen.cpp
void myScreen();
```

### 3. Register it in `src/homeScreen.cpp`

Add a label to `menuItems[]` in `main.cpp` (increment `MENU_COUNT` in `globals.h` too), then add a `case` in `navigation()`:

```cpp
// In main.cpp — menuItems array
const char* menuItems[] = { "Home", "Claude Usage", "My Screen", "Settings" };

// In globals.h — update the count
constexpr int MENU_COUNT = 4;

// In homeScreen.cpp — navigation() switch
case 2: myScreen(); break;
```

## Shared infrastructure

Every screen gets access to these via `globals.h`:

| Symbol | Type | Description |
|---|---|---|
| `tft` | `TFT_eSPI` | Display driver — 320×240, rotation 3 (landscape) |
| `spr` | `TFT_eSprite` | Sprite buffer for flicker-free sub-region drawing |
| `backLight` | `LCDBackLight` | SAMD51 TC0 PWM backlight — call `setBrightness(0–100)` |
| `drawBatteryStatus(bg)` | function | Draws charge % in top-right corner; no-op if no chassis |
| `bleSetActive(bool)` | function | Start / stop BLE advertising from any screen |
| `checkBLE()` | function | Process pending BLE writes; call in your screen's loop |
| `checkSerial()` | function | Process pending serial data; call in your screen's loop |

## Architecture notes

### globals.h as the translation-unit glue
Arduino merges all `.ino` files and auto-generates forward declarations. PlatformIO compiles each `.cpp` separately. `globals.h` takes that role: every `extern` global declaration and every cross-file function prototype lives there. Definitions live in `main.cpp`.

### Screen dispatch
`optionTest` (char in `main.cpp`) drives the top-level `switch` in `loop()`:
- `'A'` — brightness (KEY_A shortcut, bypasses menu)
- `'C'` — main menu (default)

Each sub-screen is a **blocking loop** that returns when the user exits. On return `menuNeedsRedraw = true` causes `drawMenu()` to redraw once.

**KEY_A only works at the menu level.** Once inside a sub-screen the blocking loop owns execution — `loop()` never runs, so the KEY_A check there is never reached. To support brightness adjustment inside a screen, add a `WIO_KEY_A` handler inside that screen's own loop and call `setBrightness()` directly.

### Redraw strategy
`drawMenu()` is only called when `menuNeedsRedraw` is set — never on idle — to avoid flicker from `fillScreen()`. Apply the same pattern in new screens: redraw only on state change, not every loop iteration.

### BLE lifecycle
`BLEDevice::init()` talks to the RTL8720DN and can stall if called too early. It is deferred until `millis() > 3000` in `loop()`, after the first frame has rendered in `setup()`. Any screen that wants BLE calls `bleSetActive(true)` on entry and `bleSetActive(false)` on exit; advertising stops automatically.

### Battery detection
The SAMD51 Wire bus can return a false ACK on the first probe. `batteryBegin()` follows the address probe with an actual register read to confirm the BQ27441 is present. Returns false silently on hardware without the compatible chassis — `drawBatteryStatus()` becomes a no-op.

## Library dependencies

| Library | Purpose |
|---|---|
| `Seeed_Arduino_LCD` | TFT_eSPI fork pre-configured for Wio Terminal |
| `Seeed_Arduino_FS` | Filesystem support (transitive dep of Seeed_GFX) |
| `Seeed_Arduino_rpcUnified` | RTL8720DN RPC transport (transitive dep of rpcBLE) |
| `Seeed_Arduino_rpcBLE` | BLE GATT stack for the RTL8720DN co-processor |

**TFT_eSPI conflict:** if you see "Multiple libraries found for TFT_eSPI.h", a community TFT_eSPI install is shadowing the Seeed fork. Remove any non-Seeed TFT_eSPI from your Arduino libraries folder — the Seeed fork is the one configured for this hardware.

## Button reference

| Input | Mode | Notes |
|---|---|---|
| KEY_A / B / C | `INPUT` | External pull-downs on Wio Terminal hardware |
| 5S UP / DOWN / LEFT / RIGHT / PRESS | `INPUT_PULLUP` | Active LOW |
