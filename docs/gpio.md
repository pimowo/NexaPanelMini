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
| BACKLIGHT | D3 / GPIO0 | niepotwierdzony sprzętowo, firmware nie steruje |

## Bootstrap ESP8266

Przy starcie układ wymaga poprawnych stanów:

- D8/GPIO15 musi być LOW,
- D3/GPIO0 musi być HIGH,
- D4/GPIO2 musi być HIGH.

Dlatego nie wolno zakładać, że linia backlight może bezpośrednio przyjąć PWM.
D3/GPIO0 jest pinem wyboru trybu boot.

## Backlight

`PIN_BACKLIGHT = D3` jest na razie tylko deklaracją mapowania. Firmware celowo
nie wykonuje na tym pinie `pinMode`, `digitalWrite` ani `analogWrite`.
Najpierw należy sprawdzić konkretną rewizję PCB i połączenie D3 z linią BL.

Procedura pomiaru:

1. Wyłącz zasilanie.
2. Ustaw multimetr na ciągłość lub niski zakres rezystancji.
3. Sprawdź ciągłość D3 <-> BL.
4. Ustal, czy po drodze występuje rezystor, tranzystor lub inny układ.
5. Po identyfikacji układu zmierz BL względem GND, 3V3 i 5V.
6. Nie podawaj PWM ani nie wymuszaj stanu na D3 przed potwierdzeniem PCB.
