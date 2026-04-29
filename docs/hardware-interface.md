# Hardware Interface

## Board

- `ESP32 DOIT DevKit V1`

## Display And Sensor

- `GPIO18`: TFT SCLK
- `GPIO19`: TFT MISO
- `GPIO23`: TFT MOSI
- `GPIO15`: TFT CS
- `GPIO16`: TFT DC
- `GPIO4`: TFT RESET
- `GPIO17`: display backlight PWM
- `GPIO21`: `VEML7700` SDA
- `GPIO22`: `VEML7700` SCL

Current `VEML7700` notes:

- initialized through `Wire`
- current bus speed is reduced to `50 kHz`
- used for live lux display in `Expozice` when available

## PWM Outputs

### RGB enlarger head

- `GPIO25`: red
- `GPIO26`: green
- `GPIO27`: blue

Properties:

- `10-bit PWM`
- logical maximum: `1023`

### Auxiliary PWM

- `GPIO13`: darkroom red light
- `GPIO14`: buttons backlight
- `GPIO17`: display backlight

Properties:

- `8-bit PWM`
- logical maximum: `255`

## Other Outputs

- `GPIO12`: beeper
- `GPIO2`: onboard status NeoPixel

## Buttons

- `GPIO32`: `Light On`
- `GPIO33`: `Light Off`
- `GPIO34`: `Light Timer`
- `GPIO35`: `Developer`

## Encoder

- `GPIO36`: encoder A
- `GPIO39`: encoder B
- `GPIO5`: encoder push button

## Important Electrical Notes

- `GPIO34`, `GPIO35`, `GPIO36`, and `GPIO39` are input-only.
- These input-only pins do not provide internal pull-ups.
- The encoder button on `GPIO5` uses a boot strapping pin, so avoid holding it during reset or power-up.
- Long wiring on `I2C` may require reduced bus speed and proper pull-ups.
