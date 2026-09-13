# Prompt startowy dla Codex — NexaPanel Mini

Pracujemy nad nowym projektem **NexaPanel Mini**.

## Cel

Zbudować mały panel łazienkowy na:

- Wemos D1 mini / ESP8266
- LCD TFT 2.4" 240x320
- ILI9341
- dotyk XPT2046
- ekran zawsze w pionie
- Wi-Fi
- zegar wyłącznie NTP
- bez RTC

W folderze:

`reference/NexaPanel/`

znajduje się aktualny projekt dużego panelu kuchennego **NexaPanel**.

NIE modyfikuj projektu referencyjnego.
Najpierw go przeanalizuj.

---

## Funkcje NexaPanel Mini

### 1. HOME

Ekran główny ma pokazywać:

- duży zegar
- datę
- dzień tygodnia
- temperaturę zewnętrzną
- prognozę pogody na dziś

Kliknięcie obszaru prognozy otwiera:

### 2. WEATHER_DETAILS

- prognoza na 5 dni
- prosty, czytelny widok dopasowany do 240x320
- to nadal część sekcji DOM

### 3. RADIO

Sterowanie **yoRadio** ma działać tak samo logicznie jak w dużym NexaPanel:

- aktualna stacja
- wykonawca
- tytuł
- previous
- play/pause
- next
- volume down
- volume up
- aktualny poziom głośności

Nie wymyślaj nowego API, jeśli istniejący NexaPanel już ma sprawny sposób komunikacji z yoRadio.

### 4. KOCIOŁ

Sterowanie kotłem ma bazować na istniejącej implementacji z NexaPanel.

Najpierw ustal:
- jakie dane pobiera NexaPanel,
- jakimi komendami steruje,
- jaki transport jest używany,
- które funkcje mają sens na małym ekranie.

Nie dodawaj funkcji, których nie ma w wymaganiach.

---

# Stała dolna belka

Na każdym ekranie zawsze ma być widoczna dolna belka:

`RADIO | DOM | KOCIOŁ`

Wymagania:

- trzy duże pola dotykowe
- aktywna sekcja podświetlona
- po naciśnięciu krótki stan `pressed`
- WEATHER_DETAILS nadal podświetla `DOM`
- użytkownik zawsze może jednym dotknięciem przejść do dowolnej głównej sekcji

---

# Czas

Czas pochodzi **wyłącznie z NTP**.

Nie dodawaj:
- DS3231
- RTC
- ręcznego ustawiania czasu jako podstawowej funkcji

Po pierwszej poprawnej synchronizacji ESP może utrzymywać czas systemowy podczas chwilowego zaniku Internetu.

Strefa:
- Polska
- automatyczne CET/CEST
- automatyczna zmiana lato/zima

---

# Architektura

W projekcie jest już przygotowany szkielet.

Zachowaj rozdzielenie:

- `AppState` — dane aplikacji
- `Navigation` — aktualny ekran / sekcja
- `services/` — Wi-Fi, NTP, pogoda, radio, kocioł
- `display/` — LCD i dotyk
- `ui/` — ekrany i dolna belka

Nie rób monolitycznego `main.cpp`.

UI nie może bezpośrednio wykonywać zapytań sieciowych.

Usługi pobierają dane i aktualizują `AppState`.
UI wyświetla `AppState`.

---

# Pierwsze zadanie — ANALIZA, bez dużych zmian

Najpierw:

1. Przeczytaj cały projekt `reference/NexaPanel`.
2. Znajdź:
   - sposób konfiguracji Wi-Fi,
   - sposób komunikacji z yoRadio,
   - sposób komunikacji z kotłem,
   - sposób pobierania pogody,
   - OTA / aktualizacje,
   - restart,
   - konfigurację WWW,
   - zachowanie przycisków,
   - kolory / styl / motyw,
   - wszystkie zależności sprzętowe od ESP32 / Nextion.
3. Oddziel:
   - rzeczy, które możemy przenieść 1:1,
   - rzeczy wymagające adaptacji do ESP8266,
   - rzeczy specyficzne dla Nextion, których nie przenosimy.
4. Sprawdź aktualny szkielet NexaPanelMini i wskaż błędy / ryzyka.
5. Nie przepisuj jeszcze całego projektu.

Na końcu przygotuj raport:

## A. Co znaleziono w NexaPanel
## B. Co przenosimy
## C. Czego nie przenosimy
## D. Różnice ESP32 -> ESP8266
## E. Proponowany kontrakt komunikacyjny
## F. Plan etapów implementacji
## G. Lista plików, które będą zmienione w etapie 1

Dopiero po raporcie zacznij wdrażać pierwszy mały etap.

---

# Pierwszy etap implementacji

Po analizie wykonaj tylko:

1. uruchomienie LCD ILI9341,
2. uruchomienie XPT2046,
3. kalibrację / mapowanie dotyku,
4. orientację pionową 240x320,
5. działającą nawigację:
   - RADIO
   - DOM
   - KOCIOŁ
6. aktywną dolną belkę,
7. stan `pressed`,
8. ekran HOME z danymi zastępczymi,
9. NTP,
10. podstawowy ekran diagnostyczny przez Serial.

Na tym etapie:
- nie implementuj jeszcze pełnego yoRadio,
- nie implementuj jeszcze pełnego kotła,
- nie implementuj jeszcze pogody 5-dniowej,
- nie dodawaj zbędnych bibliotek.

Po każdym etapie projekt ma się kompilować.

---

# Sprzęt / piny

Aktualne założenie:

| Funkcja | Pin |
|---|---|
| SPI SCK | D5 |
| SPI MISO | D6 |
| SPI MOSI | D7 |
| TFT CS | D8 |
| TFT D/C | D1 |
| Touch CS | D2 |
| Touch IRQ | D0 |
| SD CS | D4 |
| Backlight | D3 |

Przed uznaniem mapowania za finalne sprawdź:
- czy nie ma konfliktu pinów startowych ESP8266,
- czy dany wariant PCB Nettigo używa dokładnie tych połączeń,
- czy sterowanie podświetleniem jest faktycznie dostępne w tej rewizji PCB.

Jeżeli jest ryzyko bootstrapu ESP8266, opisz je zamiast zgadywać.

---

# Zasady pracy

- Nie modyfikuj `reference/NexaPanel`.
- Nie usuwaj działających fragmentów bez powodu.
- Nie rób dużych refaktorów przed analizą.
- Nie wymyślaj nowego protokołu, jeśli NexaPanel już ma dobry.
- Nie przenoś kodu Nextion do ILI9341.
- Zachowaj możliwie podobny UX do NexaPanel.
- Kod ma działać stabilnie 24/7.
- Pamięć ESP8266 jest ograniczona — kontroluj RAM, heap i rozmiar bibliotek.
- Unikaj dynamicznego tworzenia dużych `String`, jeśli można tego łatwo uniknąć.
- Po każdej zmianie uruchom build.
- Każdy większy krok opisz krótko przed wdrożeniem.
