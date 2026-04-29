# UI Flow

## Main Modes

- `Expozice`
- `Konfigurace`

A long press of the encoder button switches between them.

## Boot Flow

```text
                  +----------------+
                  |   Power On     |
                  +--------+-------+
                           |
                           v
                  +----------------+
                  |  BootConnect   |
                  +--------+-------+
                           |
          +----------------+----------------+
          |                                 |
          v                                 v
  network disabled                  network enabled
          |                                 |
          v                                 v
 +------------------+              +----------------------+
 |  Sit vypnuta     |              | Credentials stored ? |
 |   2 s screen     |              +----------+-----------+
 +--------+---------+                         |
          |                        +----------+----------+
          v                        |                     |
   +--------------+                v                     v
   |  Expozice    |       +----------------+     +----------------+
   +--------------+       | Try WiFi 30 s  |     | WiFi setup     |
                          +--------+-------+     | scan/password   |
                                   |             +--------+-------+
                         +---------+---------+             |
                         |                   |             |
                         v                   v             |
              +------------------+   +----------------+   |
              | WiFi connected   |   | WiFi setup     |<--+
              | info for 2 s     |   | scan/password  |
              +--------+---------+   +----------------+
                       |
                       v
                +-------------+
                |  Expozice   |
                +-------------+
```

### When network is disabled

1. startup screen
2. `Sit vypnuta` screen for `2 s`
3. `Expozice`

### When network is enabled and credentials exist

1. startup screen
2. WiFi connect attempt for up to `30 s`
3. if success: `WiFi connected` info screen for `2 s`
4. `Expozice`
5. if failure: WiFi setup flow

### When network is enabled and credentials do not exist

1. startup screen
2. WiFi setup flow

## WiFi Setup Flow

```text
+------------+      +----------------+      +-------------------+
| WifiScan   | ---> | WifiScanResult | ---> | WifiPasswordEntry |
+------------+      +--------+-------+      +---------+---------+
                             |                        |
                             | Developer = rescan     | Developer = save/connect
                             |                        v
                             |                +----------------+
                             +--------------> | WifiConnecting |
                                              +--------+-------+
                                                       |
                                         +-------------+-------------+
                                         |                           |
                                         v                           v
                               +-------------------+       +-------------------+
                               | WifiConnected 2 s |       | Back to password  |
                               +-------------------+       +-------------------+
```

### `WifiScan`

- performs scan
- then enters `WifiScanResult`

### `WifiScanResult`

- rotary encoder: next/previous SSID
- encoder short press: select SSID and open password entry
- `Developer`: rescan
- long encoder press:
  - when entered from config, return to `Konfigurace > Sit`

### `WifiPasswordEntry`

- rotary encoder: choose character
- encoder short press: insert character
- `Light On`: cursor left
- `Light Off`: delete
- `Light Timer`: cursor right
- `Developer`: save entered credentials and connect
- long encoder press:
  - when entered from config, return to `Konfigurace > Sit`

## `Expozice`

```text
+------------------------------------------------------+
| Darkroom controler                                   |
| Expozice                              [lux if valid] |
|                                                      |
| Cas:                                     <value>     |
| Kontrast:                                <value>     |
| Clona:                                   <value>     |
|                                                      |
| <exposure status left>              DEV: <n> s       |
+------------------------------------------------------+
```

Displayed values:

- top line: `Expozice` and optional lux readout from `VEML7700`
- row 1: `Cas`
- row 2: `Kontrast`
- row 3: `Clona`
- bottom left: exposure status text
- bottom right: `DEV` timer text

Controls:

- encoder short press: cycle focus `Cas -> Kontrast -> Clona -> Cas`
- encoder rotation: edit focused field
- `Light Timer`: start exposure timer
- `Light Off`: stop active exposure, or switch off manual light modes
- `Light On`: turn on white light when not exposing
- chord `Light Off` then `Light On`, release `Light On`: turn on red light
- `Developer` short press: start `DEV` timer
- hold `Developer` + rotate encoder: edit stored `DEV` time in `1 s` steps when timer is not running

```text
Encoder short press:  Cas -> Kontrast -> Clona -> Cas
Encoder rotate:       edit focused field
Light Timer:          start exposure
Light Off:            stop exposure / turn off manual light
Light On:             white light on
Off + On chord:       arm red light
Developer short:      start DEV timer
Hold Developer+rot:   adjust DEV timer setting
Long encoder press:   go to Konfigurace
```

## `Konfigurace`

Root items:

- `Osvetleni`
- `Kontrast/Expozice`
- `Sit`
- `Zpet`

```text
Konfigurace
|
+-- Osvetleni
|   +-- Cervene svetlo
|   +-- Osvetleni tlacitek
|   +-- Osvetleni displeje
|   +-- Zpet
|
+-- Kontrast/Expozice
|   +-- Barvy + korekce
|   +-- Bile svetlo
|   +-- Cervene svetlo
|   +-- Zpet
|
+-- Sit
|   +-- Sit povolena
|   +-- Pripojit ted
|   +-- Nastavit WiFi
|   +-- Zpet
|
+-- Zpet
```

### `Osvetleni`

- `Cervene svetlo`
- `Osvetleni tlacitek`
- `Osvetleni displeje`
- `Zpet`

Short press enters/leaves value editing.

### `Kontrast/Expozice`

- `Barvy + korekce`
- `Bile svetlo`
- `Cervene svetlo`
- `Zpet`

The RGB/light editors use encoder navigation plus short press to enter/leave editing.

### `Sit`

- `Sit povolena`
- `Pripojit ted`
- `Nastavit WiFi`
- `Zpet`

Behavior:

- `Sit povolena`: toggles global WiFi support
- `Pripojit ted`: connect using stored credentials
- `Nastavit WiFi`: launch the WiFi setup flow
- `Zpet`: return to root config menu
