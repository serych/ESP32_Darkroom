# Roadmap

## Implemented

- hardware abstraction layer
- PWM control for RGB head and auxiliary outputs
- TFT-based UI
- WiFi credential entry on device
- OTA startup after successful WiFi connection
- `Expozice` mode
- timed exposure
- manual white and red light modes
- `Konfigurace > Osvetleni`
- `Konfigurace > Kontrast/Expozice` editors
- `Konfigurace > Sit`
- live lux display from `VEML7700` when available
- separate `DEV` timer with final countdown feedback

## Partially Implemented

- `VEML7700` support
  - current state: live lux display only
  - still missing: explicit min/max measurement UI if that remains desired

## Planned / Open

- autoexposure
- web interface / PC-side configuration
- broader cleanup and modularization of `src/main.cpp`
- deeper documentation of calibration workflow for contrast and correction tables
