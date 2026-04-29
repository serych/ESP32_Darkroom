# Uživatelská příručka

## Start zařízení

### Pokud je síť vypnutá

- na displeji se zobrazí `Sit vypnuta`
- přibližně po `2 s` zařízení přejde do režimu `Expozice`

### Pokud je síť zapnutá

- zařízení se může pokusit připojit k WiFi
- pokud se připojení podaří, krátce zobrazí informace o připojení a potom přejde do `Expozice`
- pokud WiFi není nastavena nebo se připojení nepodaří, zařízení přejde do nastavení WiFi

## Režim Expozice

`Expozice` je hlavní pracovní obrazovka.

Na displeji se zobrazuje:

- expoziční čas
- kontrast
- clona
- vlevo dole stav expozičního časovače
- vpravo dole hodnota časovače `DEV`
- případně nahoře aktuální hodnota osvětlení z `VEML7700`, pokud je čidlo aktivní

## Enkodér

- krátký stisk: přesun výběru mezi `Cas`, `Kontrast` a `Clona`
- otáčení: změna právě vybrané hodnoty
- dlouhý stisk: přepnutí mezi `Expozice` a `Konfigurace`

## Ovládání expozice

- `Light Timer`
  - spustí expoziční časovač
- `Light Off`
  - zastaví běžící expozici
  - také vypne ruční světelné režimy
- `Light On`
  - zapne bílé světlo, pokud neběží expozice

Červené světlo:

- stiskněte `Light Off`
- při držení stiskněte `Light On`
- uvolněte `Light On`
- červené světlo se zapne

Dalším stiskem `Light Off` se znovu vypne.

## Časovač DEV

- krátký stisk `Developer`
  - spustí odpočítávání časovače pro vyvolávání
- podržení `Developer` a otáčení enkodérem
  - mění uloženou hodnotu po `1 s`
  - funguje jen tehdy, když časovač `DEV` právě neběží

Během posledních `10 s`:

- pole `DEV` bliká
- zařízení pípá každou sekundu

## Režim Konfigurace

Do režimu vstoupíte dlouhým stiskem tlačítka enkodéru z `Expozice`.

Hlavní menu:

- `Osvetleni`
- `Kontrast/Expozice`
- `Sit`
- `Zpet`

## Menu Osvětlení

Umožňuje měnit:

- jas červeného temnokomorního světla
- jas podsvícení tlačítek
- jas podsvícení displeje

## Menu Kontrast/Expozice

Umožňuje měnit:

- korekční hodnoty kontrastu
- RGB hodnoty pro jednotlivé kontrastní stupně
- RGB předvolbu bílého světla
- RGB předvolbu červeného světla

## Menu Síť

`Konfigurace > Sit` obsahuje:

- `Sit povolena`
  - globálně zapíná nebo vypíná síťové funkce
- `Pripojit ted`
  - použije uložené WiFi údaje a pokusí se připojit
- `Nastavit WiFi`
  - otevře skenování WiFi a zadávání hesla
- `Zpet`

## Nastavení WiFi

V nastavení WiFi:

- otáčením enkodéru vybíráte znak
- stiskem enkodéru znak vložíte
- `Light On` posune kurzor vlevo
- `Light Off` maže znak
- `Light Timer` posune kurzor vpravo
- `Developer` potvrdí heslo a spustí připojení

Pokud bylo nastavení WiFi otevřeno z menu sítě, dlouhý stisk enkodéru vrátí zařízení zpět do konfigurace.
