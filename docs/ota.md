# Aktualizacja firmware przez WWW (OTA)

NexaPanel Mini udostępnia aktualizację pod `http://<IP_PANELU>/update`. Endpoint
nie ma uwierzytelniania. Używaj go wyłącznie w zaufanej sieci lokalnej i nie
wystawiaj urządzenia do Internetu.

## Przygotowanie firmware

Zbuduj obraz dla środowiska `d1_mini`:

```text
pio run -e d1_mini
```

Gotowy plik znajduje się w `.pio/build/d1_mini/firmware.bin`.

## Aktualizacja

1. Otwórz `http://<IP_PANELU>/update` w przeglądarce.
2. Wybierz `firmware.bin`.
3. Kliknij **Aktualizuj** i nie odłączaj zasilania ani sieci.
4. Poczekaj, aż rzeczywisty postęp wysyłania osiągnie 100%, a strona pokaże
   komunikat `Aktualizacja zakończona pomyślnie`.
5. Panel automatycznie uruchomi się ponownie po około 1,5 sekundy.

Przy błędzie strona wyświetli przyczynę, a panel nie wykona restartu. Spróbuj
ponownie, używając świeżo zbudowanego pliku dla `d1_mini`. Jeśli OTA nadal się
nie powiedzie albo panel nie wróci do sieci, wgraj firmware przez USB; USB
pozostaje awaryjną metodą odzyskiwania.
