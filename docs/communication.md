# Komunikacja

Dokument opisuje transporty używane przez aktualny firmware NexaPanel Mini.

## Wi-Fi

`WifiService` łączy ESP8266 w trybie stacji z siecią skonfigurowaną w
`secrets.h`. Połączenie ma timeout 15 s, a ponowienia używają rosnącego
opóźnienia od 1 s do 30 s. Stan połączenia, RSSI i adres IP trafiają do
`AppState`.

## NTP

`TimeService` używa `configTime` i trzech serwerów:

- `pool.ntp.org`,
- `time.google.com`,
- `time.cloudflare.com`.

Reguła POSIX `CET-1CEST,M3.5.0,M10.5.0/3` zapewnia Europe/Warsaw oraz
automatyczny DST. Czas po synchronizacji jest utrzymywany przez zegar systemowy
ESP8266. Resynchronizacja jest konfigurowana w `config.h`.

## Weather

`WeatherService` pobiera HTTPS z Open-Meteo przez `HTTPClient`. Zapytanie
zawiera `current=temperature_2m,weather_code` oraz
`daily=weather_code,temperature_2m_max,temperature_2m_min`, strefę
`Europe/Warsaw` i `forecast_days=4`.

Firmware zapisuje temperaturę bieżącą, kod pogody, dzisiejsze MAX/MIN oraz
cztery wpisy danych dziennych. HOME używa indeksu 0, a
`WEATHER_DETAILS` pokazuje indeksy 1-3, czyli trzy przyszłe dni. Sukces powoduje
odświeżenie po 15 minutach, a błąd ponowienie po 60 sekundach. Przy błędzie
ostatnie poprawne dane pozostają w `AppState`.

## yoRadio

`RadioService` używa biblioteki Links2004 WebSockets i łączy się z
`ws://<host>:80/ws`. Po połączeniu wysyła `getindex=1`. Odbiera tekstowe ramki
JSON o strukturze `payload[]`, mapując pola `nameset`, `meta`, `playerwrap`,
`volume` i `bitrate`. Pole `meta` jest dzielone na artystę i tytuł po ` - `.

Wysyłane komendy to:

- `volm=1`,
- `volp=1`,
- `prev=1`,
- `next=1`,
- `toggle=1`.

Po 60 sekundach ciszy wysyłane jest ponownie `getindex=1`. Timeout połączenia,
timeout pierwszych danych i timeout odświeżenia powodują rozłączenie oraz
reconnect z backoffem 1, 2, 5, 10 i 30 s.

## Boiler MQTT

`BoilerService` używa MQTT przez bibliotekę `256dpi/MQTT`. Wszystkie topici są
budowane jako `MQTT_BASE_TOPIC + suffix`, gdzie obecna baza to świadomie
zachowane `kuchnia-panel` dla kompatybilności z istniejącym brokerem i
automatyzacją.

Subskrypcje:

- `kuchnia-panel/ha/state/climate`,
- `kuchnia-panel/ha/state/boiler_power`.

Po połączeniu firmware publikuje `request` do
`kuchnia-panel/ha/snapshot/request`.

Stan klimatu jest JSON-em z polami `hvac_mode`, `preset_mode`, `hvac_action`,
`target_temperature` i `current_temperature`. Stan zasilania kotła to payload
`ON` lub `OFF` na `kuchnia-panel/ha/state/boiler_power`.

Komendy:

- `kuchnia-panel/ha/command/climate/hvac_mode`: `heat` albo `off`,
- `kuchnia-panel/ha/command/climate/preset_mode`: `comfort` albo `sleep`,
- `kuchnia-panel/ha/command/climate/temperature`: liczba z jedną cyfrą po
	przecinku w zakresie 15.0-25.0,
- `kuchnia-panel/ha/command/boiler_power`: `ON` albo `OFF`.

Połączenie MQTT ma keepalive 15 s i reconnect z backoffem od 1 s do 30 s.
