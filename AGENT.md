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
- The current `src/main.cpp` is no longer the original hardware-test application; it now contains the working timer UI with WiFi setup, OTA, exposure mode, light measurement display, developer timer, and several config editors.
- On startup it plays three tones and briefly drives the RGB light head and status LED through red, green, and blue.
- The TFT display is initialized in landscape orientation and uses the larger font already validated on hardware.
- A global network enable flag is stored in config NVS.
- If networking is disabled, boot shows `Sit vypnuta` for `2 s` and then enters `Expozice` without trying to use WiFi.
- If networking is enabled and WiFi credentials are stored in NVS, the device attempts to connect for `30 s` and shows the countdown on screen.
- If networking is enabled but no credentials are stored, or if the connection times out, the device enters WiFi setup mode, scans visible SSIDs, and shows the scan results on the display.
- The WiFi list is browsed with the rotary encoder and an SSID is selected with the encoder push button.
- Password entry is implemented on-device:
  - rotary encoder selects a character
  - encoder button inserts the selected character
  - `Light On` moves the cursor left
  - `Light Off` deletes at the cursor
  - `Light Timer` moves the cursor right
  - `Developer` commits the password and starts connection
- WiFi credentials are saved in NVS using `Preferences`.
- After successful connection the display shows SSID, IP address, and RSSI for `2 s`, and Arduino OTA is started.
- `platformio.ini` already contains commented OTA upload settings intended to be uncommented after the first serial upload of OTA-enabled firmware.
- After the short post-connect screen, the normal working screen is `Expozice`.
- A long press of the encoder button switches between `Expozice` and `Konfigurace`.
- In `Expozice`, a short press of the encoder button cycles focus between exposure time, contrast, and aperture.
- In `Expozice`, rotating the encoder edits the currently focused value:
  - exposure time follows a logarithmic sequence from `1.0 s` upward by roughly `sqrt(2)` steps up to `10 min`
  - contrast range is `0..10`
  - aperture range is `0..6`
- Exposure time shown on screen is already prepared for multiplication by a contrast correction coefficient table.
- A contrast correction coefficient table for `0..10` exists in code and is currently initialized to neutral `1.0` values.
- A contrast-to-RGB table for `0..10` exists in code and is currently initialized to `{3,5,5}` for all entries.
- RGB table entries are interpreted as logarithmic PWM indices for the 10-bit head output with the mapping:
  - `0 -> 0`
  - `1 -> 2`
  - `2 -> 4`
  - `3 -> 8`
  - ...
  - `10 -> 1023`
- Simple timed exposure is implemented:
  - `Light Timer` starts exposure when no exposure is running
  - `Light Off` stops a running exposure early
  - the RGB head is turned on only during the active exposure and uses the color from the current contrast RGB table entry
- During exposure the remaining time is updated on a larger dedicated status line without redrawing the entire screen.
- The top `Expozice` line can show live `VEML7700` lux readings while in exposure mode if the sensor initializes correctly.
- `VEML7700` currently uses `I2C` at `50 kHz` for compatibility with the current wiring; if the sensor does not initialize, the UI simply leaves the measurement area blank.
- Exposure end or manual stop plays a distinct double beep using the user-tuned end-tone constants in `src/main.cpp`.
- A separate developer-bath countdown timer is implemented in `Expozice`:
  - the right side of the bottom status line shows `DEV: <n> s`
  - default value is `90 s`
  - the stored range is `10..300 s` in `1 s` steps
  - a short `Developer` button press starts the countdown
  - holding the `Developer` button and rotating the encoder changes the stored time when the timer is not running
  - during the last `10 s` it beeps at `2093 Hz`
  - the `DEV` label blinks by alternating between normal and inverted blue styling during those last `10 s`
  - seconds `10..2` use short beeps and the final `1 s` uses a `500 ms` beep
- Manual latched light modes are also implemented in exposure mode:
  - white light color is currently configurable in code and initialized to `{10,10,10}`
  - red light color is currently configurable in code and initialized to `{10,0,0}`
  - `Light On` turns on the white light if no exposure is running
  - `Light Off` turns off the white light
  - pressing `Light Off` and then `Light On` together arms the red-light chord; releasing `Light On` turns on the red light
  - the next `Light Off` press turns the red light off
- `Konfigurace` is no longer only a placeholder:
  - `Osvetleni` supports editing darkroom red light, buttons backlight, and display backlight
  - `Kontrast/Expozice` supports editing the contrast correction table and RGB tables, plus white and red light RGB presets
  - `Sit` supports:
    - enabling/disabling networking globally
    - connecting immediately using stored WiFi credentials
    - launching the existing WiFi SSID/password setup flow from the configuration menu
    - returning back to `Konfigurace` after the setup flow when it was launched from there
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
1. WiFi connectivity and implementation of OTA - done
2. working modes and menu - done
3. exposure mode, timer, apperture, contrast, display values and basic buttons functions - done
4. config menu, red light and backlights values settings - done
5. light head colors definitions, contrast - color - exposure correction table - done at the current editor level
6. VEML7700 measurements - live lux display in exposure mode done; min/max display still pending
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
- Status: implemented and working on hardware, including OTA upload after first serial flash.
## 2. Working modes and menu
- There will be two main modes of operation: Exposure mode and Config mode
    - In exposure mode it will be posible to set exposure time, contrast and apperture using rottary encoder and buttons will be used to switch light head ON, OFF and start the timer
    - In config mode the device will show the menu and values which will be set by user (brightnesses, colors of head light etc.)
- Status: implemented in the current branch as `Expozice` default mode and `Konfigurace` entered by long press of the encoder button.
## 3. Exposure mode, timer, apperture, contrast, display values and basic buttons functions
- `Expozice` is the default post-connect screen.
- Short encoder press cycles focus between exposure time, contrast, and aperture.
- Rotary movement edits the focused value.
- Timed exposure is started by `Light Timer` and can be stopped early by `Light Off`.
- The light head is driven during exposure using the current contrast RGB table entry.
- Manual white and red light modes for non-exposure work are already wired as described in the current firmware behavior section.
- The top line can show current lux from `VEML7700` when the sensor initializes correctly.
- A separate `Developer` countdown timer is shown on the right side of the bottom status line, can be started by a short `Developer` press, and its setting can be changed by holding `Developer` and rotating the encoder.
- Status: implemented and hardware-tested at the current basic level.
## 4. Config menu, darkroom lighting settings
- Menu 'Konfigurace' with subitems:
    - 'Osvětlení' 
    - 'Kontrast/Expozice'
    - 'Sit'
    - 'Zpet' - back to 'Expozice'
In 'Osvetleni' there will be 3 items:
- 'Cervene svetlo' - value 0 to 7 PWM with 1b shifted from the right to the left (1, 3, 7, 15, ... 255) -> GPIO13
- 'Osvetleni tlacitek' - the same principle -> GPIO14
- 'Osvetleni displeje' - the same -> GPIO17
- 'Zpet' - parent menu level  
- Status: implemented and stored in NVS.
## 5. Color and correction table settings
- menu 'Kontrast/Expozice'
    - 'Barvy + korekce'
    - 'Bile svetlo'
    - 'Cervene svetlo'
    - 'Zpet'
We need to set contrast correction values and light colors for all the contrast steps in range 0..10 in the 'Barvy + korekce' item. 
- Correction values are in range 0 to 4 with 2 decimal numbers steps. The default value is 1.0
- Default color values are {3,5,5} (range 0..10 as described in Current firmware behavior.) Show the color on the light head during the setting. All the values will be written to NVS
- We will set the white light color in 'Bile svetlo' (no correction value, just the color)
- We will set the red light color in 'Cervene svetlo' 
Also in these two cases show the actual color during the settings and write the values to the NVS 
- Status: implemented at the current editor level and stored in NVS.

## Network config menu

- `Konfigurace > Sit` currently provides:
  - `Sit povolena` checkbox-like toggle
  - `Pripojit ted` to connect with stored credentials immediately
  - `Nastavit WiFi` to launch the existing SSID scan and password-entry flow
  - `Zpet`
- When WiFi setup is launched from this menu, a long encoder press returns to `Konfigurace` instead of jumping to `Expozice`.
- Boot-time WiFi behavior is gated by the global network enable flag.


