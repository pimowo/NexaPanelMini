# Maintenance prompt - NexaPanel Mini

Ten plik jest instrukcją utrzymaniową dla publicznego repozytorium. Źródłem
prawdy jest aktualny kod firmware oraz README i dokumenty w `docs/`. Folder
`reference/` jest materiałem historycznym, ignorowanym przez Git i nie jest
zależnością builda.

## Zasady

- Nie dodawaj funkcji bez wyraźnego wymagania.
- Nie zmieniaj zaakceptowanego UI, fontów, kolorów, pozycji, ikon, clippingu ani
  partial redraw bez jednoznacznego błędu.
- Nie implementuj backlightu, dopóki połączenie D3/GPIO0 z PCB nie zostanie
  potwierdzone pomiarem.
- Nie zmieniaj MQTT, WebSocket, pogody, touch ani NTP bez potwierdzonej potrzeby
  i testu regresji.
- Zachowaj `FW_VERSION "0.1.0-dev"` do czasu decyzji o backlighcie i końcowego
  soak testu. Nie twórz taga `v1.0.0` przed tym etapem.
- Nie modyfikuj `reference/`.

## Architektura

Utrzymuj przepływ `Services -> AppState -> UI` oraz
`Touch -> UiManager -> Navigation / RadioService / BoilerService`. Pętla ma
pozostać nieblokująca poza kontrolowanym requestem pogodowym. Zachowaj cache
 ekranów, partial redraw i auto-home po 30 sekundach bez nowego dotyku.

## Konfiguracja i build

- Sekrety są tylko w lokalnym, ignorowanym `include/secrets.h`.
- Ustawienia niesekretne są w `include/config.h`, a GPIO w `include/pins.h`.
- Buduj środowisko `d1_mini` przez PlatformIO.
- Po zmianach wykonaj clean build i zwykły build, sprawdź RAM, Flash, warningi
  oraz `git diff --check`.
- Biblioteki muszą pozostać przypięte do wersji zapisanych w `platformio.ini`.

## Dokumentacja

Aktualizuj dokumentację razem z kodem. Opisuj wyłącznie zachowania widoczne w
implementacji, nie plany z pierwotnego szkicu. Przed zmianą sprawdź `git status`
i nie usuwaj cudzych zmian.
