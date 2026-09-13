# NexaPanel Mini

Mały panel łazienkowy oparty o:

- Wemos D1 mini / ESP8266
- LCD TFT 2.4" 240x320 ILI9341
- dotyk XPT2046
- orientacja pionowa
- Wi-Fi
- czas wyłącznie z NTP

## Główne ekrany

- HOME
  - duży zegar
  - data
  - dzień tygodnia
  - temperatura zewnętrzna
  - prognoza na dziś
  - kliknięcie prognozy -> widok 5-dniowy
- RADIO
  - sterowanie yoRadio jak w dużym NexaPanel
- KOCIOŁ
  - sterowanie kotłem jak w dużym NexaPanel

## Stała dolna belka

Zawsze widoczna:

`RADIO | DOM | KOCIOŁ`

Aktywna sekcja jest podświetlona. Po dotknięciu przycisk ma krótki stan "pressed".

## Referencyjny NexaPanel

Do folderu:

`reference/NexaPanel/`

wklej cały aktualny projekt panelu kuchennego NexaPanel.

Projekt referencyjny ma służyć Codexowi do analizy istniejących:
- API i komunikacji z yoRadio,
- obsługi kotła,
- Wi-Fi,
- OTA,
- konfiguracji,
- stylu UI i zachowania przycisków.

NexaPanelMini nie może być zależny kompilacyjnie od folderu `reference/`.
