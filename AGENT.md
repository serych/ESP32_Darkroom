# AGENT.md

## Project overview

This repository contains a small PlatformIO firmware project for an `ESP32 DOIT DevKit V1` using the Arduino framework and a TFT display driven through a vendored copy of `TFT_eSPI`.

Current application entrypoint:

- `src/main.cpp`

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
- If display or pin behavior needs to change, first inspect the active `TFT_eSPI` setup files before modifying application logic.
- Preserve `platformio.ini` environment names unless the task explicitly requires a new board or environment.

## Current firmware behavior

As of the current workspace state:

- The firmware initializes serial at `115200`.
- It initializes a `TFT_eSPI` display and sets rotation to landscape (`1`).
- It writes two strings to the display using free fonts from `Free_Fonts.h`.
- It drives `TFT_LED` with `analogWrite`.

Be careful with:

- Character rendering: `src/main.cpp` currently contains non-ASCII text (`"ÄŚas"`), which may indicate encoding or font coverage issues.
- Pin definitions: `TFT_LED` is expected to come from the selected `TFT_eSPI` configuration, not from local project code.

## Common commands

Run these from the repository root:

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

If `pio` is unavailable, use PlatformIO through the installed IDE integration or the full executable available on the machine.

## Change guidance

- For UI changes on the display, prefer keeping layout constants near the top of `src/main.cpp`.
- For hardware-specific fixes, document which board, display driver, and pin mapping the change assumes.
- When changing fonts or text rendering, verify that the selected font actually supports the required glyphs.
- Keep examples and diagnostics from `lib/TFT_eSPI/examples/` as reference material only; do not copy large example code into the main application without reducing it to the required behavior.

## Verification expectations

After code changes, prefer:

1. `pio run`
2. `pio run -t upload` if hardware is connected
3. `pio device monitor -b 115200` for runtime verification

If hardware is unavailable, state clearly that only static or build-level verification was performed.
