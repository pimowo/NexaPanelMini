# Architektura NexaPanel Mini

NexaPanel Mini jest samodzielnym firmware ESP8266. Folder `reference/` jest
materiałem historycznym i nie jest zależnością kompilacji.

## Warstwy

### Hardware

- `DisplayDriver` - konfiguracja i dostęp do TFT ILI9341,
- `TouchDriver` - odczyt i mapowanie XPT2046,
- `pins.h` - mapowanie GPIO oraz ostrzeżenia bootstrapów.

### Core

- `AppState` - wspólny stan czasu, pogody, radia, kotła i Wi-Fi,
- `Navigation` - bieżący ekran i aktywna sekcja.

### Services

- `WifiService`,
- `TimeService`,
- `WeatherService`,
- `RadioService`,
- `BoilerService`.

Usługi są właścicielami komunikacji sieciowej. Aktualizują `AppState`, a UI nie
zna hostów, topiców ani szczegółów bibliotek transportowych.

`BoilerService` pełni dwie role MQTT na jednym połączeniu:

- komunikacja kotła (stan i komendy),
- diagnostyka panelu i Home Assistant MQTT Discovery.

Role używają oddzielnych namespace topiców.

### UI

- `UiManager` - pętla dotyku, auto-home, nawigacja i routing odświeżania,
- `HomeScreen`,
- `RadioScreen`,
- `BoilerScreen`,
- `BoilerTemperatureScreen`,
- `WeatherScreen`,
- `BottomBar`.

## Przepływ danych

```text
Wi-Fi / NTP / Open-Meteo / WebSocket / MQTT
						  |
					  Services
						  |
					  AppState
						  |
						  UI
```

Akcje użytkownika przepływają tak:

```text
Touch -> UiManager -> Navigation / RadioService / BoilerService
```

## Pętla główna

`setup()` inicjalizuje sprzęt, usługi i UI. `loop()` wywołuje aktualizacje usług,
UI i diagnostykę, po czym wykonuje krótkie opóźnienie 5 ms. Timeouty Wi-Fi,
MQTT, yoRadio, pogody i auto-home są nieblokujące. Sam request pogody jest
krótką operacją HTTP kontrolowaną timeoutem klienta.

## Renderowanie

Ekran jest rysowany ponownie przy zmianie nawigacji. Przy pozostaniu na ekranie
`update()` porównuje dane z cache i odświeża tylko zmienione regiony. Dolna
belka ma własny cache aktywnej sekcji i nie jest rysowana na
`BOILER_TEMPERATURE`, który posiada własny dolny przycisk `WRÓĆ`. Projekt nie
alokuje dużego framebufferu; rysuje bezpośrednio do TFT.

Auto-home po 30 sekundach bez nowego dotyku wraca z każdego ekranu poza HOME do
HOME. Nowy dotyk resetuje licznik.

Na ekranie `BOILER_TEMPERATURE` przyciski `-` i `+` korzystają z nieblokującego
hold/repeat (500 ms / 225 ms) w pętli `UiManager`.
