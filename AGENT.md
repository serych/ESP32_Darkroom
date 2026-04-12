# AGENT.md

## Project overview

This project is a photographic darkroom timer built on `ESP32 DOIT DevKit V1` and `PlatformIO`.
It uses:

- three 10-bit PWM channels for the RGB enlarger light head
- 8-bit PWM for darkroom red light, display backlight, and button backlight
- four dedicated front-panel buttons
- rotary encoder with button
- a beeper for timer feedback
- a `VEML7700` light sensor on I2C
- a `TFT_eSPI` display with pins already defined in `lib/TFT_eSPI/User_Setup.h`

Current application entrypoints:

- `src/main.cpp`
- `src/darkroom_hw.cpp`

Public hardware header:

- `include/darkroom_hw.h`

Build configuration:

- `platformio.ini`

Vendored library:

- `lib/TFT_eSPI`

The workspace contains a Git repository, but it may include in-progress user changes. Do not revert unrelated edits.

## Repository structure

- `src/`: firmware source files
- `include/`: project headers
- `lib/`: local libraries; `TFT_eSPI` is checked in here
- `test/`: PlatformIO test area
- `.pio/`: generated build artifacts
- `.vscode/`: editor settings

## Working rules

- Prefer editing project code in `src/` and `include/`.
- Treat `.pio/` as generated output. Do not hand-edit files there.
- Avoid broad changes inside `lib/TFT_eSPI` unless the task explicitly requires library customization.
- If display or pin behavior needs to change, inspect the active `TFT_eSPI` setup first.
- Keep the application hardware mapping in `include/darkroom_hw.h` synchronized with the real schematic.
- Preserve `platformio.ini` environment names unless the task explicitly requires a new board or environment.

## Current firmware behavior

As of the current workspace state:

- The firmware initializes serial at `115200`.
- It initializes a hardware abstraction layer in `include/darkroom_hw.h` and `src/darkroom_hw.cpp`.
- The HAL exposes helper functions for RGB head PWM, darkroom red PWM, display backlight PWM, buttons backlight PWM, button reads, encoder reads, and both blocking and non-blocking beeper tone output.
- The current `src/main.cpp` is a dedicated hardware test application, not the final timer UI.
- On startup it plays three tones and briefly drives the RGB light head and status LED through red, green, and blue.
- The TFT display is initialized in landscape orientation and shows a `Testing hardware` screen with live values for button backlight PWM, display backlight PWM, encoder value, and the last input event.
- `kButtonLightOff` turns the status LED and RGB light head red while held.
- `kButtonLightOn` turns the status LED and RGB light head green while held, and each press advances the button-backlight PWM through a logarithmic-style table.
- `kButtonDeveloperTimer` turns the status LED and RGB light head blue while held.
- `kButtonLightTimer` is still read and beeped for input testing, but it does not currently drive the RGB light head.
- The encoder increments and decrements a displayed test variable and plays short click tones for both directions.
- The encoder push button advances the display-backlight PWM through the same logarithmic-style table.
- `DarkroomHw::updateBeep()` is expected to be called from `loop()` so short click sounds can end without blocking input scanning.
- The currently tested hardware is working, including the RGB light head outputs on `GPIO25`, `GPIO26`, and `GPIO27`.

Be careful with:

- `GPIO34` and `GPIO35` are input-only and do not provide internal pull-ups.
- `GPIO36` and `GPIO39` are also input-only and do not provide internal pull-ups.
- Display SPI and control pins are defined in `lib/TFT_eSPI/User_Setup.h`; application pin proposals must not conflict with them.
- Avoid non-ASCII UI text unless the selected font has the needed glyph coverage.

## Proposed ESP32 pin assignment

Display and sensor:

- `GPIO18`: TFT SCLK
- `GPIO19`: TFT MISO
- `GPIO23`: TFT MOSI
- `GPIO15`: TFT CS
- `GPIO16`: TFT DC
- `GPIO4`: TFT RESET
- `GPIO17`: display backlight PWM
- `GPIO21`: `VEML7700` SDA
- `GPIO22`: `VEML7700` SCL

PWM outputs:

- `GPIO25`: RGB light head red, 10-bit PWM
- `GPIO26`: RGB light head green, 10-bit PWM
- `GPIO27`: RGB light head blue, 10-bit PWM
- `GPIO13`: darkroom red light, 8-bit PWM
- `GPIO14`: buttons backlight, 8-bit PWM

Other outputs:

- `GPIO12`: beeper tone output
- `GPIO2`: onboard NeoPixel status LED data

Buttons:

- `GPIO32`: magnifier light head ON
- `GPIO33`: magnifier light head OFF
- `GPIO34`: magnifier light head TIMER start
- `GPIO35`: developer bath timer start

Rotary encoder:

- `GPIO36`: encoder A
- `GPIO39`: encoder B
- `GPIO5`: encoder push button

This uses the `mathertel/RotaryEncoder` state-machine library for A/B decoding.

## Common commands

Run these from the repository root:

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

If `pio` is unavailable, use PlatformIO through the installed IDE integration or the full executable available on the machine.

## Change guidance

- For hardware changes, update `include/darkroom_hw.h` first and keep the implementation in `src/darkroom_hw.cpp` aligned with it.
- For PWM-controlled outputs, keep RGB at 10-bit and auxiliary channels at 8-bit unless there is a measured reason to change.
- Keep in mind that the RGB light head uses 10-bit PWM values from `0` to `1023`; copying 8-bit values like `64` directly to the head will produce only a very dim output.
- For buttons on `GPIO34` and `GPIO35`, and encoder signals on `GPIO36` and `GPIO39`, assume external pull-ups in hardware and active-low logic in software.
- `GPIO5` is a boot strapping pin, so avoid holding the encoder button during reset or power-up.
- When changing test behavior in `src/main.cpp`, preserve user-tuned tone frequencies and durations unless the task explicitly asks to change them.
- Keep examples from `lib/TFT_eSPI/examples/` as reference material only; do not copy large example code into the main application.

## Verification expectations

After code changes, prefer:

1. `pio run`
2. `pio run -t upload` if hardware is connected
3. `pio device monitor -b 115200` for runtime verification

If hardware is unavailable, state clearly that only static or build-level verification was performed.

# Development steps
0. creation of HW layer and testing HW - done
1. WiFi connectivity and implementation of OTA
2. working modes and menu
3. exposure mode, timer, apperture, contrast, display values and basic buttons functions
4. light head colors definitions, contrast - color - exposure correction table
5. red light and backlights values settings
6. VEML7700 measurements - display min, max values
7. autoexposure implementation
8. web interface

# Detailed development steps
## 0. HW testing - done
## 1. Wifi and OTA
- if WiFi is not connected up to timeout time after the reset (30s for now, later it will be possible to set the value), scan SSIDs and show them on the display
- let the user choose SSID and enter the password using virtual keyboard on the display, rotary encoder to choose character and other buttons to move the cursor, delete the character and enter commit the password written (ON = <, OFF = delete, Timer = > and Develop = commit)
- write the values (SSID and passwd) to non volatile memory
- connect to the choosen AP, show IP and signal strength on the display and begin the OTA
- (choosing of SSID will later also be one of the config menu items) 
## 2. Working modes and menu
- There will be two main modes of operation: Exposure mode and Config mode
    - In exposure mode it will be posible to set exposure time, contrast and apperture using rottary encoder and buttons will be used to switch light head ON, OFF and start the timer 
    - In config mode the device will show the menu and values which will be set by user (brightnesses, colors of head light etc.) 