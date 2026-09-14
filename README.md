# NexaPanel Mini

NexaPanel Mini to firmware dla małego panelu ESP8266 z Wemos D1 mini,
wyświetlaczem ILI9341 240x320 w orientacji pionowej i kontrolerem dotyku
XPT2046. Urządzenie korzysta z Wi-Fi, NTP, Open-Meteo, yoRadio WebSocket oraz
MQTT dla kotła. Działa samodzielnie, bez zależności od Home Assistant po stronie
interfejsu użytkownika; Home Assistant jest używany tylko jako zgodny punkt
końcowy kontraktu MQTT kotła.

## Funkcje

### HOME

- zegar, dzień tygodnia i data,
- aktualna pogoda, ikona i opis,
- dzisiejsze MAX/MIN,
- aktualna temperatura,
- dotknięcie obszaru pogody otwiera `WEATHER_DETAILS`.

### RADIO

- yoRadio przez WebSocket,
- stacja, artysta, utwór i poziom głośności,
- po 20 s bez skutecznego połączenia z yoRadio zamiast `VOL xxx` pojawia się
	`Błąd`,
- poprzednia stacja, play/pause i następna stacja,
- przytrzymanie przycisku głośności.

### BOILER

- MQTT,
- zasilanie kotła,
- HVAC `heat/off`,
- płomień jako status grzania,
- temperatura aktualna i zadana,
- dotknięcie pełnego obszaru temperatur otwiera ekran ręcznej zmiany zadanej,
- tryby `comfort`, `sleep` i stan manualny (oba presety nieaktywne).

### BOILER_TEMPERATURE

- nagłówek `Temperatura`,
- centralnie prezentowana temperatura zadana,
- duże pola `-` i `+`,
- zakres 15.0-25.0 C i krok 0.1 C,
- przytrzymanie: start po 500 ms, repeat co 225 ms,
- pojedynczy dolny przycisk `<- WRÓĆ` powracający do ekranu kotła,
- auto-home po 30 s bez dotyku jak na pozostałych ekranach.

### WEATHER_DETAILS

Widok pokazuje dokładnie trzy przyszłe dni: pełny polski dzień tygodnia z datą,
ikonę oraz MAX/MIN. Dane wejściowe zawierają także dzień bieżący używany na
ekranie HOME.

### Auto-home

Każdy ekran poza HOME wraca do HOME po 30 sekundach bez nowego dotyku. Każdy
nowy dotyk resetuje timer.

## Dolna belka

Fizyczna kolejność od lewej strony ekranu to:

`KOCIOŁ | HOME | RADIO`

Belka jest widoczna na ekranach głównych. `WEATHER_DETAILS` pozostaje częścią
sekcji HOME, a ekran `BOILER_TEMPERATURE` ma własny dolny przycisk `WRÓĆ`
zamiast standardowej belki.

## Architektura

Przepływ danych ma postać:

`Services -> AppState -> UI`

Akcje dotyku przechodzą przez:

`Touch -> UiManager -> Navigation / RadioService / BoilerService`

Usługi obsługują transporty i aktualizują `AppState`, a warstwa UI tylko
prezentuje stan oraz wysyła intencje użytkownika.

## Komunikacja

- Wi-Fi w trybie stacji ESP8266,
- NTP z trzema serwerami i regułą POSIX dla Polski,
- Open-Meteo przez HTTPS/HTTPClient: `current` oraz `daily`,
- yoRadio WebSocket pod `/ws`,
- MQTT dla stanu i sterowania kotłem.

## Home Assistant MQTT Discovery

Panel publikuje własne MQTT Discovery jako osobne urządzenie diagnostyczne
`NexaPanel Mini` w namespace `nexapanel-mini`, niezależnie od topiców kotła.

Encje Discovery:

- `Wi-Fi RSSI` (sensor, dBm, diagnostic),
- `Uptime` (sensor, sekundy, diagnostic),
- `Firmware` (sensor, diagnostic),
- `Restart` (button, komenda `PRESS`).

Availability panelu:

- topic: `nexapanel-mini/status`,
- payload online: `online` (retained),
- payload offline: `offline` przez LWT (retained).

Discovery config jest publikowany jako retained po każdym poprawnym połączeniu
MQTT (bez publikowania co pętlę). Telemetria RSSI i uptime jest publikowana po
połączeniu i okresowo co 30 s.

Szczegóły kontraktów znajdują się w [docs/communication.md](docs/communication.md).

## Konfiguracja

### `include/secrets.h`

Lokalny, ignorowany przez Git plik zawierający wyłącznie:

- SSID Wi-Fi,
- hasło Wi-Fi,
- użytkownika MQTT,
- hasło MQTT.

Utwórz go na podstawie [include/secrets.example.h](include/secrets.example.h).

### `include/secrets.example.h`

Śledzony w Git szablon prywatnych makr wymaganych lokalnie.

### `include/config.h`

Zwykłe ustawienia użytkownika i systemu: hosty, porty, ścieżka WebSocket,
współrzędne pogody, timeouty, reconnect/backoff, timezone, serwery NTP oraz
kalibracja dotyku. Plik zawiera osobne sekcje MQTT dla kotła i panelu.

### `include/pins.h`

Mapowanie GPIO sprzętu. Podświetlenie LCD jest podłączone stale do zasilania i
nie jest sterowane przez ESP8266.

## Build

Wymagane są VS Code i PlatformIO. Projekt używa środowiska `d1_mini`.

```text
pio run
pio run --target upload
pio device monitor --baud 115200
```

W VS Code dostępne są także zadania budowania i uploadu PlatformIO.

## Hardware / pinout

Aktualne mapowanie i ostrzeżenia bootstrapów opisuje [docs/gpio.md](docs/gpio.md).
Najważniejsze: D8/GPIO15 musi być LOW podczas startu, a D3/GPIO0 i D4/GPIO2
muszą być HIGH. Podświetlenie LCD nie używa linii sterowania z ESP8266.

## Time

Strefa czasu to `CET-1CEST,M3.5.0,M10.5.0/3`, czyli automatyczne CET/CEST dla
Europe/Warsaw. Firmware nie używa RTC ani ręcznego przełączania UTC+1/UTC+2.

## Aktualny status

- firmware: `0.1.0-dev`,
- projekt: kandydat do release przed v1.0.0,
- backlight jest sprzętowo zasilany stale i nie jest sterowany przez firmware,
- końcowy 24-godzinny soak test nie został jeszcze wykonany.

Folder `reference/` jest materiałem historycznym ignorowanym przez Git i nie
jest wymagany do kompilacji ani działania NexaPanel Mini.
