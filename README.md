# ESP32 Darkroom

Firmware for a photographic darkroom timer built on `ESP32 DOIT DevKit V1` with a TFT UI, RGB enlarger light control, WiFi/OTA support, and local configuration stored in NVS.

## What It Does

- controls a three-channel RGB enlarger light head with `10-bit PWM`
- controls darkroom red light, display backlight, and buttons backlight with `8-bit PWM`
- provides two main UI modes: `Expozice` and `Konfigurace`
- supports timed exposure and a separate `DEV` countdown timer
- supports optional WiFi setup and OTA updates
- reads a `VEML7700` light sensor when available

## Repository Layout

- `src/`: firmware source
- `include/`: public project headers
- `lib/`: vendored libraries and local dependencies
- `docs/`: project documentation
- `AGENT.md`: live technical snapshot for future coding work
- `platformio.ini`: PlatformIO build configuration

## Build And Run

From the repository root:

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

If `pio` is not available in `PATH`, use the installed PlatformIO executable or the IDE integration.

## Documentation Index

- [Architecture](docs/architecture.md)
- [UI Flow](docs/ui-flow.md)
- [Config Storage](docs/config-storage.md)
- [Hardware Interface](docs/hardware-interface.md)
- [Timers And Signals](docs/timers-and-signals.md)
- [User Manual](docs/user-manual.md)
- [Uzivatelska Prirucka](docs/uzivatelska-prirucka.md)
- [Roadmap](docs/roadmap.md)
- [Known Issues](docs/known-issues.md)

## Current State

The current firmware already implements:

- working `Expozice` mode
- working `Konfigurace` submenus for lighting, contrast/exposure tables, and networking
- optional WiFi enable/disable gating at boot
- OTA startup after successful WiFi connection
- `DEV` timer editing and countdown feedback

For the most accurate current implementation notes, see [AGENT.md](AGENT.md).
