# Architecture

## Overview

The firmware is currently centered around `src/main.cpp`, plus a hardware abstraction layer in `darkroom_hw.*`.

```text
+------------------------------+
|          src/main.cpp        |
|------------------------------|
| UI states and drawing        |
| Buttons and encoder logic    |
| Exposure timer               |
| DEV timer                    |
| WiFi setup and connect       |
| OTA startup                  |
| Preferences/NVS persistence  |
| VEML7700 handling            |
+--------------+---------------+
               |
               v
+------------------------------+
|    include/darkroom_hw.h     |
|------------------------------|
| Pin map                      |
| HAL API                      |
| Button/encoder types         |
+--------------+---------------+
               |
               v
+------------------------------+
|     src/darkroom_hw.cpp      |
|------------------------------|
| PWM init and writes          |
| GPIO reads                   |
| Encoder polling              |
| Beeper control               |
+------------------------------+
```

Main responsibilities:

- `src/main.cpp`
  - UI modes and screen drawing
  - button and encoder behavior
  - exposure timer logic
  - developer timer logic
  - WiFi setup and connection flow
  - OTA startup
  - config persistence in NVS
  - `VEML7700` initialization and lux reads
- `include/darkroom_hw.h`
  - public hardware API
  - pin assignments
  - button state and encoder types
- `src/darkroom_hw.cpp`
  - PWM setup
  - raw GPIO/button handling
  - encoder polling
  - beeper tone control

## Runtime Structure

The runtime is a polling loop:

1. update encoder state
2. update active non-blocking beeps
3. handle OTA if active
4. update WiFi boot flow if needed
5. update exposure and developer timers
6. refresh exposure lux display when applicable
7. read buttons and process edge-triggered actions
8. handle encoder rotation and button press behavior

There is no RTOS task split in the application layer at the moment. The code is state-driven and uses `millis()` for timers.

```text
loop()
  |
  +--> updateEncoder()
  +--> updateBeep()
  +--> ArduinoOTA.handle()            if OTA active
  +--> boot WiFi state handling       if in BootConnect
  +--> exposure timer refresh
  +--> developer timer refresh
  +--> lux refresh in Expozice
  +--> readButtons()
  +--> handleButtonEdges()
  +--> handleEncoder()
  +--> delay(5)
```

## Main UI Modes

- `BootConnect`
  - initial startup state
  - may transition to WiFi connect flow, WiFi setup flow, or directly to `Expozice`
- `WifiScan`
  - scanning visible SSIDs
- `WifiScanResult`
  - choose an SSID
- `WifiPasswordEntry`
  - enter WiFi password with rotary encoder and buttons
- `WifiConnecting`
  - blocking connect attempt for the entered credentials
- `WifiConnected`
  - short success/info screen
- `ExposureMode`
  - main working screen
- `ConfigMode`
  - nested configuration menu

## Config Areas

`ConfigMode` currently includes:

- `Osvetleni`
  - darkroom red light level
  - buttons backlight level
  - display backlight level
- `Kontrast/Expozice`
  - contrast correction coefficients
  - RGB table per contrast grade
  - white light RGB preset
  - red light RGB preset
- `Sit`
  - global network enable flag
  - immediate connect using stored credentials
  - launch of the existing WiFi setup flow

## State Persistence

Configuration and WiFi credentials are stored in `Preferences` namespaces:

- `config`
  - exposure-related values
  - lighting values
  - network enable flag
  - `DEV` timer setting
  - RGB tables and correction table
- `wifi`
  - `ssid`
  - `password`

See [Config Storage](config-storage.md) for details.

## Notes On Future Refactoring

The current code works, but `src/main.cpp` now contains multiple responsibilities. A practical next refactor would be:

- `ui/` for drawing and screen formatting
- `timers/` for exposure and developer timers
- `network/` for WiFi/OTA/setup flow
- `storage/` for NVS serialization

That refactor is optional; the current single-file structure is still manageable, but it is no longer minimal.
