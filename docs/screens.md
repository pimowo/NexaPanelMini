# Ekrany i nawigacja

Ekran ma rozmiar 240x320 w orientacji pionowej. Standardowa dolna belka
zajmuje dolne 54 px. Jej kolejność od lewej to:

`KOCIOŁ | HOME | RADIO`

Aktywna sekcja jest rysowana kolorem akcentu. Kod nie implementuje osobnego
widocznego stanu po dotknięciu. Wyjątkiem jest ekran `BOILER_TEMPERATURE`,
który używa własnego przycisku `WRÓĆ` zamiast standardowej belki.

## HOME

HOME pokazuje zegar, dzień tygodnia, datę, aktualną pogodę, ikonę, opis oraz
dzisiejsze MAX/MIN i temperaturę bieżącą. Dotknięcie obszaru prognozy powyżej
belki otwiera `WEATHER_DETAILS`.

Źródła danych HOME:

- ikona/opis/MAX/MIN: Open-Meteo,
- temperatura: HA `ha/shared/outside_temperature/state` z fallbackiem
	Open-Meteo,
- ciśnienie: HA `ha/shared/outside_pressure/state` z fallbackiem Open-Meteo.

Znaczenie etykiety:

- `Aktualnie`: zarówno temperatura, jak i ciśnienie pochodzą ze świeżego HA,
- `Aktualnie*`: co najmniej jedna z wartości używa fallbacku Open-Meteo.

## WEATHER_DETAILS

Widok pokazuje trzy przyszłe dni, czyli wpisy prognozy od indeksu 1 do 3.
Każdy wpis zawiera pełny polski dzień tygodnia z datą, ikonę oraz temperatury
MAX/MIN. Nie ma osobnego aktywnego podświetlenia dla tego widoku; nawigacja
traktuje go jako sekcję HOME.

## RADIO

Ekran pokazuje stację, artystę, utwór, stan odtwarzania i głośność. Dostępne
akcje to poprzednia stacja, play/pause, następna stacja oraz zmniejszanie i
zwiększanie głośności. Przytrzymanie przycisku głośności powtarza komendę.

Jeśli po starcie albo po rozłączeniu przez ponad 20 s brak skutecznego
połączenia WebSocket, pole głośności pokazuje `Błąd` zamiast `VOL xxx`.
Po reconnect UI automatycznie wraca do `VOL xxx`.

## BOILER

Ekran pokazuje stan zasilania, HVAC, status grzania/płomienia, temperaturę
aktualną i zadaną oraz tryb comfort/sleep. Sterowanie wysyła komendy przez
`BoilerService`.

Kliknięcie pełnego prostokątnego obszaru temperatur (od górnej linii etykiet
`Aktualna`/`Zadana` do dołu wartości i na pełnej szerokości ekranu) przechodzi
do `BOILER_TEMPERATURE`.

Renderowanie presetu:

- COMFORT: słońce aktywne (żółte), księżyc szary,
- SLEEP: księżyc aktywny (niebieski), słońce szare,
- MANUAL: oba preset icons szare.

Stan MANUAL wynika z rzeczywistego `preset_mode` z MQTT, bez heurystyki
temperatury.

## BOILER_TEMPERATURE

Dedykowany ekran ręcznej zmiany temperatury zadanej kotła:

- nagłówek `Temperatura` jak na `WEATHER_DETAILS`,
- centralna wartość `xx.x°C`,
- duże pola `-` i `+` po bokach,
- krok 0.1 C w zakresie 15.0-25.0 C,
- przytrzymanie: start 500 ms, repeat 225 ms,
- natychmiastowa aktualizacja LCD po kliknięciu,
- po nadejściu stanu MQTT wartość z `AppState` nadpisuje lokalny podgląd,
- szeroki przycisk `<- WRÓĆ` na dole prowadzi do `BOILER`.

## TOUCH_CALIBRATION

Tryb startowy kalibracji XPT2046 (poza standardową nawigacją ekranów):

- uruchamia się automatycznie, gdy brak poprawnej kalibracji w EEPROM,
- można go wymusić przez przytrzymanie dotyku przez ok. 3 sekundy podczas
	startu,
- zbiera 5 punktów kalibracyjnych metodą dotknij-puść,
- mapowanie osi jest wykrywane automatycznie (swap/invert),
- po zapisaniu kalibracji wraca do normalnego startu UI.

## Auto-home

Każdy ekran poza HOME wraca do HOME po 30 sekundach bez nowego dotyku. Nowy
dotyk resetuje licznik. Mechanizm jest nieblokujący i działa w pętli
`UiManager`. Nie dotyczy trybu `TOUCH_CALIBRATION`.

## Odświeżanie

Pierwsze wejście na ekran wykonuje pełny rysunek jego obszaru. Późniejsze
zmiany danych używają cache i odświeżają tylko zmienione fragmenty.
