# Panel kuchenny - wymagania

## Sprzęt

- ESP32-S3 Super Mini
- Nextion NX4832T035 Basic 480x320
- UART Nextion:
  - TX ESP32: GPIO4
  - RX ESP32: GPIO5
  - 9600 baud
  - 8N1

## Architektura komunikacji

### yoRadio

Komunikacja bezpośrednia przez WebSocket.

Kod ma bazować na sprawdzonej implementacji z projektu yoPilot.

Panel steruje jednym radiem.

### Home Assistant

Komunikacja przez MQTT.

Urządzenie ma używać MQTT Discovery, aby encje panelu mogły pojawiać się automatycznie w Home Assistant.

## Nextion

Obecny projekt HMI pozostaje.

Aktualizacja ekranu ma być event-driven.

Nie wykonywać pełnego redraw bez potrzeby.

Pełna synchronizacja dozwolona:
- po uruchomieniu,
- po resecie Nextiona,
- po zmianie strony,
- po ponownym połączeniu źródła danych.

## Konfiguracja

Wszystkie ustawienia użytkownika znajdują się w:

include/config.h

Nie tworzyć konfiguracji rozproszonej po modułach.

Komentarze w kodzie mają być po polsku.

## Timeout UI

Jeżeli użytkownik znajduje się na stronie RADIO lub KOCIOŁ i przez określony czas nie dotknie ekranu, panel ma wrócić do START.

Czas ustawiany w config.h.

Każdy dotyk resetuje licznik bezczynności.

Na stronie START timeout nie wykonuje żadnej akcji.