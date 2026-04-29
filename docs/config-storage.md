# Config Storage

## Namespaces

The firmware uses two `Preferences` namespaces:

- `config`
- `wifi`

## `config` Keys

### Lighting

- `red_light`
  - darkroom red light level
  - range: `0..7`
- `btn_light`
  - buttons backlight level
  - range: `0..7`
- `disp_light`
  - display backlight level
  - range: `0..7`

### Exposure UI

- `exposure`
  - index into logarithmic exposure table
- `contrast`
  - range: `0..10`
- `aperture`
  - range: `0..6`

### Contrast Tables

- `corr_tbl`
  - `float[11]`
  - correction coefficient per contrast step
  - current allowed range: `0.0..4.0`
- `rgb_tbl`
  - `ContrastRgbSetting[11]`
  - RGB log-PWM steps per contrast step

### Fixed Color Presets

- `white_rgb`
  - `ContrastRgbSetting`
- `red_rgb`
  - `ContrastRgbSetting`

### Network

- `net_en`
  - `bool`
  - global enable/disable flag for networking

### Developer Timer

- `dev_time`
  - `uint16_t`
  - default: `90`
  - allowed range after load/clamp: `10..300`

## `wifi` Keys

- `ssid`
  - WiFi SSID
- `password`
  - WiFi password

## Validation Rules

Current load-time clamping:

- lighting levels: `0..7`
- contrast: `0..10`
- aperture: `0..6`
- `DEV` timer: `10..300`
- correction table entries: `0.0..4.0`
- RGB log steps: `0..10`

If values are outside expected ranges, the firmware clamps them during load.
