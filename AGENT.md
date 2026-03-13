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

This workspace is not a Git repository. Do not assume Git-based workflows are available.

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
- It exposes helper functions for RGB head PWM, darkroom red PWM, display backlight PWM, buttons backlight PWM, button reads, and beeper tone output.
- It initializes the TFT display in landscape orientation and shows a basic pin summary screen.

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
- `GPIO2`: TFT DC
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

- `GPIO16`: beeper tone output

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
- For buttons on `GPIO34` and `GPIO35`, and encoder signals on `GPIO36` and `GPIO39`, assume external pull-ups in hardware and active-low logic in software.
- `GPIO5` is a boot strapping pin, so avoid holding the encoder button during reset or power-up.
- Keep examples from `lib/TFT_eSPI/examples/` as reference material only; do not copy large example code into the main application.

## Verification expectations

After code changes, prefer:

1. `pio run`
2. `pio run -t upload` if hardware is connected
3. `pio device monitor -b 115200` for runtime verification

If hardware is unavailable, state clearly that only static or build-level verification was performed.
