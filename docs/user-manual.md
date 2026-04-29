# User Manual

## Startup

### If network is disabled

- the display shows `Sit vypnuta`
- after about `2 s` the device enters `Expozice`

### If network is enabled

- the device may try to connect to WiFi
- if connection succeeds, it briefly shows connection information and then enters `Expozice`
- if connection is not configured or fails, it enters WiFi setup

## Exposure Mode

`Expozice` is the main working screen.

The screen shows:

- exposure time
- contrast
- aperture
- bottom-left status for the exposure timer
- bottom-right `DEV` countdown value
- optional lux value on the top line if the light sensor is active

## Encoder

- short press: move focus between `Cas`, `Kontrast`, and `Clona`
- rotation: change the focused value
- long press: switch between `Expozice` and `Konfigurace`

## Exposure Controls

- `Light Timer`
  - starts the exposure timer
- `Light Off`
  - stops a running exposure
  - also turns off manual light modes
- `Light On`
  - turns on white light when no exposure is running

Red light:

- press `Light Off`
- while holding it, press `Light On`
- release `Light On`
- red light turns on

The next `Light Off` turns it off again.

## Developer Timer

- short press `Developer`
  - starts the developer countdown
- hold `Developer` and rotate encoder
  - changes the stored time in `1 s` steps
  - only works while the developer timer is not running

During the last `10 s`:

- the `DEV` field blinks
- the device beeps each second

## Configuration Mode

Enter by long-pressing the encoder button from `Expozice`.

Main menu:

- `Osvetleni`
- `Kontrast/Expozice`
- `Sit`
- `Zpet`

## Lighting Menu

Allows changing:

- darkroom red light brightness
- buttons backlight brightness
- display backlight brightness

## Contrast/Exposure Menu

Allows changing:

- contrast correction values
- RGB values per contrast step
- white light RGB preset
- red light RGB preset

## Network Menu

`Konfigurace > Sit` contains:

- `Sit povolena`
  - enables or disables all network behavior
- `Pripojit ted`
  - uses stored WiFi credentials and tries to connect
- `Nastavit WiFi`
  - opens WiFi scan and password entry
- `Zpet`

## WiFi Setup

In WiFi setup:

- rotate encoder to choose a character
- press encoder to insert character
- `Light On` moves the cursor left
- `Light Off` deletes
- `Light Timer` moves cursor right
- `Developer` confirms the password and starts connection

When WiFi setup was entered from the network config menu, a long encoder press returns back to configuration.
