# GPIO - Wemos D1 mini

| Funkcja | Pin | Uwagi |
|---|---|---|
| SPI SCK | D5 / GPIO14 | TFT, touch i ewentualne SD |
| SPI MISO | D6 / GPIO12 | TFT, touch i ewentualne SD |
| SPI MOSI | D7 / GPIO13 | TFT, touch i ewentualne SD |
| TFT CS | D8 / GPIO15 | musi być LOW podczas boot |
| TFT DC | D1 / GPIO5 |  |
| TFT RST | -1 | reset sprzętowy nieużywany |
| Touch CS | D2 / GPIO4 |  |
| Touch IRQ | D0 / GPIO16 | sterownik działa przez polling; IRQ nie jest używane |
| SD CS | D4 / GPIO2 | obecnie nieużywany, musi być HIGH podczas boot |
| BACKLIGHT | - | podłączony sprzętowo stale do zasilania, bez sterowania z ESP |

## Bootstrap ESP8266

Przy starcie układ wymaga poprawnych stanów:

- D8/GPIO15 musi być LOW,
- D3/GPIO0 musi być HIGH,
- D4/GPIO2 musi być HIGH.

Dlatego nie wolno zakładać, że linia backlight może bezpośrednio przyjąć PWM.
D3/GPIO0 jest pinem wyboru trybu boot.

## Backlight

Podświetlenie LCD jest sprzętowo zasilane na stałe i nie ma linii sterowania z
ESP8266 w aktualnym układzie.

## Kalibracja dotyku

Ponowną kalibrację XPT2046 można wymusić bez dodatkowego GPIO: przytrzymanie
dotyku przez około 3 sekundy podczas startu uruchamia tryb kalibracji.
