# Architektura NexaPanel Mini

## Założenia

NexaPanel Mini jest osobnym firmware, ale ma korzystać z istniejących i sprawdzonych rozwiązań z NexaPanel.

Warstwy:

1. Hardware / display
2. UI
3. AppState
4. Services
5. Transport / integracje

UI nie powinno znać szczegółów HTTP, WebSocket, MQTT ani Home Assistant.
Usługi aktualizują `AppState`.
UI wyświetla stan i generuje akcje użytkownika.

## Najważniejsza zasada

Nie kopiować ślepo całego NexaPanel.

Najpierw:
- przeanalizować,
- ustalić kontrakty,
- przenieść tylko potrzebne fragmenty,
- zachować zachowanie i kompatybilność.
