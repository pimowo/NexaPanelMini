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
zawiera `current=temperature_2m,pressure_msl,weather_code` oraz
`daily=weather_code,temperature_2m_max,temperature_2m_min`, strefę
`Europe/Warsaw` i `forecast_days=4`.

Firmware zapisuje temperaturę bieżącą, ciśnienie bieżące, kod pogody,
dzisiejsze MAX/MIN oraz cztery wpisy danych dziennych. HOME używa indeksu 0, a
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

UI radia pokazuje `Błąd` w miejscu `VOL xxx`, jeśli od startu albo od ostatniego
rozłączenia przez ponad 20 s brak skutecznego połączenia WebSocket. Kryterium
opiera się o realny stan połączenia, nie o sam brak metadanych. Po odzyskaniu
połączenia `Błąd` znika automatycznie.

## MQTT kotła

`BoilerService` używa MQTT przez bibliotekę `256dpi/MQTT`. Wszystkie topici są
budowane jako `BOILER_MQTT_BASE_TOPIC + suffix`, gdzie obecna baza to świadomie
zachowane `kuchnia-panel` dla kompatybilności z istniejącym brokerem i
automatyzacją.

Subskrypcje:

- `kuchnia-panel/ha/state/climate`,
- `kuchnia-panel/ha/state/boiler_power`.

Dodatkowo klient MQTT panelu subskrybuje temperaturę zewnętrzną HA:

- `ha/shared/outside_temperature/state`.

Oraz ciśnienie zewnętrzne HA:

- `ha/shared/outside_pressure/state`.

Payload jest prostą wartością liczbową bez jednostki i bez JSON: temperatura
w °C, ciśnienie w hPa. To logiczna wartość
encji `sensor.temperatura_zewnetrzna`; firmware nie jest powiązany z fizycznym
źródłem tej encji po stronie HA.

Po połączeniu firmware publikuje `request` do
`kuchnia-panel/ha/snapshot/request`.

Stan klimatu jest JSON-em z polami `hvac_mode`, `preset_mode`, `hvac_action`,
`target_temperature` i `current_temperature`. Stan zasilania kotła to payload
`ON` lub `OFF` na `kuchnia-panel/ha/state/boiler_power`.

`preset_mode` jest źródłem prawdy dla stanu presetu UI:

- `comfort` -> COMFORT,
- `sleep` -> SLEEP,
- `manual` (lub inny nierozpoznany preset) -> MANUAL.

Stan MANUAL nie jest wykrywany heurystyką temperatury.

Komendy:

- `kuchnia-panel/ha/command/climate/hvac_mode`: `heat` albo `off`,
- `kuchnia-panel/ha/command/climate/preset_mode`: `comfort` albo `sleep`,
- `kuchnia-panel/ha/command/climate/temperature`: liczba z jedną cyfrą po
	przecinku w zakresie 15.0-25.0,
- `kuchnia-panel/ha/command/boiler_power`: `ON` albo `OFF`.

Ekran `BOILER_TEMPERATURE` używa tej samej komendy temperatury. Klik `-`/`+`
zmienia lokalny podgląd o 0.1 C i od razu publikuje komendę, a późniejszy stan
MQTT pozostaje źródłem prawdy i nadpisuje podgląd.

Połączenie MQTT ma keepalive 15 s i reconnect z backoffem od 1 s do 30 s.

Ważność temperatury zewnętrznej HA:

- brak poprawnej aktualizacji przez 15 minut oznacza `stale` i fallback do
	Open-Meteo (gwiazdka przy wartości) na ekranie HOME,
- payload `unavailable`/`unknown`/niepoprawny jest traktowany jako invalid,
- przy rozłączeniu MQTT panel natychmiast przechodzi na fallback Open-Meteo.

Ważność ciśnienia zewnętrznego HA:

- brak poprawnej aktualizacji przez 15 minut oznacza `stale` i fallback do
	Open-Meteo (gwiazdka przy wartości) na ekranie HOME,
- payload `unavailable`/`unknown`/niepoprawny jest traktowany jako invalid,
- przy rozłączeniu MQTT panel natychmiast przechodzi na fallback Open-Meteo.

HA musi publikować oba topici okresowo, np. co 5 minut, także gdy wartość
się nie zmienia, oraz przy zmianie stanu i starcie HA. Retained wiadomość
nie zastępuje heartbeat: timeout 15 minut biegnie od ostatniej poprawnej
wiadomości odebranej przez panel. Finalna walidacja v1.0.0 potwierdziła publikację przy zmianie stanu, starcie
Home Assistant oraz heartbeat co 5 minut. Pięć minut jest krótsze niż timeout
15 minut. Firmware nie zarządza konfiguracją automatyzacji HA.

Praktyczna konfiguracja znajduje się w [docs/home-assistant.md](home-assistant.md).

## MQTT NexaPanel Mini / Home Assistant Discovery

Panel ma osobny namespace MQTT:

- baza: `PANEL_MQTT_BASE_TOPIC = nexapanel-mini`.

Topici stanu panelu:

- `nexapanel-mini/status` (availability),
- `nexapanel-mini/rssi/state`,
- `nexapanel-mini/uptime/state`,
- `nexapanel-mini/firmware/state`.

Topic komend panelu:

- `nexapanel-mini/restart/set` z payload `PRESS`.

### Availability i LWT

Klient MQTT ustawia LWT na `nexapanel-mini/status` z payload `offline`
(retained, QoS 1). Po poprawnym połączeniu panel publikuje `online`
(retained, QoS 1). Dzięki temu HA widzi poprawny stan urządzenia także po
nieoczekiwanym zerwaniu połączenia.

### Discovery

Discovery config jest publikowany jako retained pod topicami:

- `homeassistant/sensor/nexapanel-mini/rssi/config`,
- `homeassistant/sensor/nexapanel-mini/uptime/config`,
- `homeassistant/sensor/nexapanel-mini/firmware/config`,
- `homeassistant/button/nexapanel-mini/restart/config`.

Publikacja discovery następuje po poprawnym połączeniu MQTT (oraz po reconnect).
Nie jest wykonywana co pętlę.

### Telemetria panelu

RSSI i uptime są publikowane po połączeniu oraz okresowo co 30 s.
Firmware jest publikowane jako retained przy połączeniu.
