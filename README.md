# NexaPanel Mini

Mały panel łazienkowy oparty o:

- Wemos D1 mini / ESP8266
- LCD TFT 2.4" 240x320 ILI9341
- dotyk XPT2046
- orientacja pionowa
- Wi-Fi
- czas wyłącznie z NTP

## Konfiguracja lokalna

Skopiuj `include/secrets.example.h` jako `include/secrets.h` i uzupełnij
wartości lokalne. Plik `include/secrets.h` jest ignorowany przez Git.

```cpp
#define YORADIO_HOST_VALUE "192.168.1.100"
#define YORADIO_PORT_VALUE 80
#define WEATHER_LATITUDE_VALUE "52.2297"
#define WEATHER_LONGITUDE_VALUE "21.0122"
```

Adres yoRadio i współrzędne Open-Meteo są pobierane wyłącznie z tego pliku.
Puste współrzędne wyłączają pobieranie pogody bez blokowania pozostałych usług.

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
