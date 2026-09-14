# Ekrany i nawigacja

Ekran ma rozmiar 240x320 w orientacji pionowej. Dolna belka zajmuje dolne 54
px i jest rysowana na każdym ekranie. Jej kolejność od lewej to:

`KOCIOŁ | HOME | RADIO`

Aktywna sekcja jest rysowana kolorem akcentu. Kod nie implementuje osobnego
widocznego stanu po dotknięciu.

## HOME

HOME pokazuje zegar, dzień tygodnia, datę, aktualną pogodę, ikonę, opis oraz
dzisiejsze MAX/MIN i temperaturę bieżącą. Dotknięcie obszaru prognozy powyżej
belki otwiera `WEATHER_DETAILS`.

## WEATHER_DETAILS

Widok pokazuje trzy przyszłe dni, czyli wpisy prognozy od indeksu 1 do 3.
Każdy wpis zawiera pełny polski dzień tygodnia z datą, ikonę oraz temperatury
MAX/MIN. Nie ma osobnego aktywnego podświetlenia dla tego widoku; nawigacja
traktuje go jako sekcję HOME.

## RADIO

Ekran pokazuje stację, artystę, utwór, stan odtwarzania i głośność. Dostępne
akcje to poprzednia stacja, play/pause, następna stacja oraz zmniejszanie i
zwiększanie głośności. Przytrzymanie przycisku głośności powtarza komendę.

## BOILER

Ekran pokazuje stan zasilania, HVAC, status grzania/płomienia, temperaturę
aktualną i zadaną oraz tryb comfort/sleep. Sterowanie wysyła komendy przez
`BoilerService`.

## Auto-home

Każdy ekran poza HOME wraca do HOME po 30 sekundach bez nowego dotyku. Nowy
dotyk resetuje licznik. Mechanizm jest nieblokujący i działa w pętli
`UiManager`.

## Odświeżanie

Pierwsze wejście na ekran wykonuje pełny rysunek jego obszaru. Późniejsze
zmiany danych używają cache i odświeżają tylko zmienione fragmenty.
