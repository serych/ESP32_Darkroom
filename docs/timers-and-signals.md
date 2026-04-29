# Timers And Signals

## Exposure Timer

Purpose:

- controls enlarger exposure duration

Behavior:

- started by `Light Timer` in `Expozice`
- stopped early by `Light Off`
- remaining time is shown on the left side of the bottom status line
- end of exposure triggers a distinct double beep

Source parameters:

- exposure base value comes from the logarithmic exposure table
- displayed and executed value is multiplied by the contrast correction coefficient

## Developer Timer

Purpose:

- separate countdown for developer bath timing

Behavior:

- displayed as `DEV: <n> s` on the right side of the bottom status line
- default value is `90 s`
- configurable in `1 s` steps
- valid range: `10..300 s`
- started by a short `Developer` button press in `Expozice`
- adjusted by holding `Developer` and rotating the encoder while the timer is not running

## Final 10 Seconds

During the last `10 s` of the developer timer:

- the `DEV` label blinks by alternating normal and inverted rendering
- the beeper uses `2093 Hz`
- seconds `10..2` use short beeps
- second `1` uses a long `500 ms` beep

## UI Tones

Current sound categories:

- short UI click tones for buttons
- rotary step tones
- exposure-end double beep
- developer-timer final countdown tones

The tone constants are currently defined directly in `src/main.cpp`.
