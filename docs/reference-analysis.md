# Analiza referencyjnego NexaPanel

> Materiał historyczny i projektowy. Opisuje wcześniejszą analizę folderu
> `reference/NexaPanel/`; nie jest specyfikacją aktualnego runtime NexaPanel
> Mini i nie jest używany podczas builda. Folder `reference/` jest ignorowany
> przez Git.

Aktualną dokumentacją firmware są README oraz pliki w `docs/`. Poniższe notatki
zachowano jako ślad decyzji przy przenoszeniu kontraktów.

Analiza dotyczy kodu w `reference/NexaPanel/`. Projekt referencyjny pozostaje niezmieniony. Wartości poufne zapisane w jego `include/config.h` nie są powielane w tym raporcie.

## A. Co znaleziono w NexaPanel

### Wi-Fi i konfiguracja

- ESP32 pracuje jako STA, domyślnie z DHCP; obsługiwane są także statyczny IP, brama, maska i dwa DNS.
- Połączenie ma timeout 15 s i kontrolowany backoff 1, 2, 5, 10, 30 s. Automatyczny reconnect biblioteki jest wyłączony, a stan trafia do `AppState`.
- Konfiguracja runtime jest zapisywana w NVS przez `Preferences`, ma wersję schematu, sumę kontrolną, walidację i wartości domyślne.
- WWW udostępnia status, konfigurację, import/eksport JSON, reset konfiguracji, restart oraz upload firmware i pliku HMI.
- Endpointy: `GET /`, `GET /nextion`, `GET /api/status`, `GET /api/config`, `GET /api/config/export`, `POST /api/config`, `POST /api/config/import`, `POST /api/config/reset`, `POST /api/restart`, `POST /api/firmware/upload?...`, `POST /nextion/upload?...`.

### Czas

- Wyłącznie SNTP/NTP; brak RTC.
- Strefa jest ustawiana regułą POSIX dla Polski: `CET-1CEST,M3.5.0,M10.5.0/3`.
- Serwery: pool.ntp.org, time.google.com i time.cloudflare.com; resynchronizacja co 6 godzin.
- Czas uznaje się za poprawny od roku 2024. Po synchronizacji używany jest systemowy zegar ESP podczas chwilowego braku sieci.

### yoRadio

- Transport: bezpośredni WebSocket, domyślnie `ws://<host>:80/ws`, biblioteka Links2004 WebSockets.
- Po połączeniu panel wysyła `getindex=1` i czeka maksymalnie 5 s na pierwsze rozpoznane dane.
- Komendy są tekstowe: `prev=1`, `toggle=1`, `next=1`, `volm=1`, `volp=1`.
- Odpowiedź jest JSON-em z tablicą `payload`, której wpisy mają `id` i `value`.
- Używane identyfikatory: `nameset`, `meta`, `fmt`, `playerwrap`, `volume`, `bitrate`, `rssi`, `heap`, `bass`, `middle`, `trebble`, `balance`.
- `meta` jest dzielone na wykonawcę i tytuł po `" - "`; rozpoznawane są też stany connecting/loading/buffering, stopped, paused i error.
- Sesja ma timeouty, walidację danych, czyszczenie nieaktualnego stanu i backoff 1, 2, 5, 10, 30 s.

### Kocioł i dane domu

- Transport: MQTT przez brokera i kontrakt Home Assistant, biblioteka 256dpi/MQTT.
- Wszystkie topiki są budowane jako `<base><suffix>`.
- Stan pogody: `/ha/state/weather`, payload będący pojedynczą temperaturą.
- Stan klimatu: `/ha/state/climate`, JSON z `hvac_mode`, `preset_mode`, `hvac_action`, `target_temperature`, `current_temperature`.
- Stan zasilania kotła: `/ha/state/boiler_power`, payload `ON` lub `OFF`.
- Stan okien: `/ha/state/windows`; dodatkowo istnieją kalendarz i odpady, ale nie należą do wymagań Mini.
- Komendy: `/ha/command/climate/hvac_mode` (`heat`/`off`), `/ha/command/climate/preset_mode` (`comfort`/`sleep`), `/ha/command/climate/temperature` (liczba z jedną cyfrą po przecinku), `/ha/command/boiler_power` (`ON`/`OFF`).
- Zakres temperatury zadanej to 15,0-25,0 C, krok 0,1 C. Przytrzymanie zaczyna powtarzanie po 500 ms co 250 ms; oczekiwanie na potwierdzenie MQTT trwa 2,5 s.
- Sensowny zakres Mini: stan online, zasilanie, aktualna i zadana temperatura, grzanie/bezczynność, tryb comfort/sleep oraz +/- temperatury. Dane kalendarza, odpadów i diagnostyczne encje HA są poza wymaganiami.

### OTA, restart i WWW

- Firmware OTA jest wysyłany przez własny nieblokujący serwer HTTP do `Update`, po czym wykonywany jest kontrolowany restart.
- ESP32 używa partycji OTA i mechanizmu potwierdzania/rollback obrazu.
- Aktualizacja Nextiona implementuje osobny protokół UART, zmianę baud rate, bloki 4096 B i ACK.
- Restart jest wykonywany przez `POST /api/restart` z krótkim opóźnieniem, aby odpowiedź HTTP mogła zostać wysłana.
- Strona WWW znajduje się w PROGMEM, pokazuje diagnostykę i edytuje Wi-Fi, MQTT, yoRadio oraz UI. Hasła nie są eksportowane.

### Przyciski, UI i styl

- Nextion raportuje osobno PRESS i RELEASE. Akcja pojedyncza jest wykonywana przy zwolnieniu; głośność i temperatura obsługują przytrzymanie.
- Każdy dotyk aktualizuje licznik bezczynności. RADIO i KOCIOŁ wracają po timeout do START; START pozostaje bez zmian.
- Render jest event-driven, z cache wartości i pełnym odświeżeniem tylko przy starcie, zmianie strony lub odzyskaniu źródła danych.
- HMI ma strony START, RADIO, BOILER, BOOT i UPDATE. Kolory istotne dla logiki to jasny tekst 61407, akcent kalendarza 65504 i czerwony alarm 63488. Większa część wyglądu jest zaszyta w pliku HMI, którego nie ma w repozytorium.

### Zależności sprzętowe

- ESP32-S3: `Preferences`/NVS, `esp_sntp`, `esp_mac`, `esp_ota_ops`, `esp_heap_caps`, FreeRTOS, PSRAM, podwójny UART/USB CDC i partycje OTA.
- Nextion: UART, parser ramek `0x65/0x66/0x88`, kolejka poleceń, identyfikatory stron/komponentów, obrazy HMI i updater `.tft`.
- Referencja nie używa ILI9341, XPT2046 ani współdzielonej magistrali SPI.

## B. Co przenosimy

### 1:1 na poziomie kontraktu i zachowania

- Kontrakt WebSocket yoRadio, komendy, parser pól, timeout pierwszych danych i backoff.
- Sufiksy MQTT, formaty payloadów kotła i walidację zakresów.
- Regułę polskiej strefy czasowej, trzy serwery NTP i okres resynchronizacji.
- Rozdzielenie `AppState -> services -> UI`, stan received/valid dla danych zewnętrznych oraz nieblokujące pętle usług.
- Semantykę PRESS/RELEASE, przytrzymania oraz powrotu do HOME po bezczynności.
- Walidację konfiguracji i zasadę nieujawniania haseł w API/eksporcie.

### Po adaptacji

- Wi-Fi z ręcznym backoff, ale przez `ESP8266WiFi` i `WiFi.hostname()`.
- Runtime config z NVS/Preferences na EEPROM lub LittleFS z wersją i CRC; dopiero w późniejszym etapie.
- MQTT i WebSocket po pomiarze RAM; bufory muszą być mniejsze i statyczne tam, gdzie to praktyczne.
- Firmware OTA przez API ESP8266 `Update`; bez rollbacku właściwego dla partycji ESP32.
- Diagnostykę: `ESP.getFreeHeap()`, reset reason ESP8266, uptime, Wi-Fi i status NTP, bez PSRAM i FreeRTOS stack HWM.
- UI jako bezpośredni rysunek TFT, zachowujący hierarchię i logikę dużego panelu, ale nie pikselową kopię Nextiona.

## C. Czego nie przenosimy

- `NextionDriver`, `NextionUi`, `NextionIds`, upload `.tft`, protokół UART i zasoby/numery obrazów HMI.
- Kod PSRAM, FreeRTOS, ESP32 USB CDC, `esp_*` i schemat partycji ESP32.
- Kalendarz, odpady, sterowanie oknami i dodatkowe encje diagnostyczne, ponieważ nie ma ich w wymaganiach Mini.
- Ręczne ustawianie czasu ani RTC.
- Pełne yoRadio, kocioł, prognozę 5-dniową i WWW/OTA w etapie 1.

## D. Różnice ESP32 -> ESP8266

- ESP8266 ma znacznie mniej RAM i brak PSRAM; duże `StaticJsonDocument<1536>`, MQTT buffer 2048, rozbudowana strona WWW i wiele `String` nie powinny być kopiowane bez pomiarów.
- Brak `Preferences`; trwała konfiguracja wymaga EEPROM lub LittleFS.
- Inne API Wi-Fi/hostname, SNTP callbacków, reset reason i OTA; brak FreeRTOS API użytego w referencji.
- Jedna sprzętowa magistrala SPI jest współdzielona przez TFT, dotyk i opcjonalną SD. Nieaktywne CS muszą pozostawać w stanie HIGH.
- `D8/GPIO15` jest pinem bootstrap wymagającym LOW podczas startu. TFT CS na D8 jest typowym, ale ryzykownym połączeniem, jeśli moduł lub rezystory wymuszą HIGH.
- `D3/GPIO0` i `D4/GPIO2` muszą być HIGH przy starcie. Obciążenie backlight na D3 albo SD CS na D4 może uniemożliwić normalny boot. `D0/GPIO16` nie obsługuje zwykłych przerwań GPIO, ale XPT2046 może być odpytywany; IRQ nie powinien być wymagany do działania.
- Repozytorium zawiera tylko deklarowane założenie pinów i ostrzeżenie o rewizji. Nie zawiera numeru rewizji, schematu Nettigo ani opisu `BL MOD`; publiczne wyszukiwanie nie dało jednoznacznego dokumentu dla tej konkretnej płytki. Dlatego D3 jako sterowanie podświetleniem oraz całe mapowanie pozostają warunkowe do kontroli ciągłości/oznaczeń PCB. Firmware nie powinien aktywnie sterować D3 przed tym potwierdzeniem.
## Aktualny kontrakt

Rzeczywiste, wdrożone kontrakty są opisane w [docs/communication.md](communication.md).
W szczególności aktualny firmware pobiera pogodę bezpośrednio z Open-Meteo,
pokazuje trzy przyszłe dni i używa aktywnego kontraktu MQTT opartego o bazę
`kuchnia-panel`. Nie należy używać tego dokumentu jako listy przyszłych etapów.
