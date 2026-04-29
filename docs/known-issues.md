# Known Issues

## Light Sensor Reliability

- `VEML7700` may fail to initialize on long `I2C` wiring.
- The bus is currently reduced to `50 kHz`.
- If initialization fails, the lux field is simply not shown.

## WiFi Connect Flow

- The entered-credentials connect attempt still uses a blocking wait loop.
- It works, but it is less elegant than the rest of the polling-based firmware.

## Large Application File

- `src/main.cpp` currently contains most application logic.
- The code is still workable, but future growth would benefit from splitting UI, timers, storage, and networking into separate files.

## UI Font Constraints

- The current UI uses the already validated TFT font setup.
- Czech diacritics are intentionally avoided in most runtime text to avoid glyph coverage issues.
