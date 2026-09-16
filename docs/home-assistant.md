# Integracja z Home Assistant

NexaPanel Mini komunikuje się z Home Assistant wyłącznie przez MQTT. Nie używa REST API Home Assistant i nie potrzebuje tokena HA.

## Kierunki komunikacji

```text
                    MQTT broker
 Home Assistant  <--------------->  NexaPanel Mini
      |                                  |
      +--> temperatura, ciśnienie        +--> komendy kotła
      +--> climate / boiler state        +--> MQTT Discovery
                                           +--> RSSI, uptime, firmware
                                           +--> availability, restart

 HA temperature / pressure
             |
             v
      NexaPanel Mini -- brak/stale --> Open-Meteo fallback
```

Home Assistant publikuje temperaturę zewnętrzną, ciśnienie zewnętrzne, stan `climate` kotła i stan zasilania kotła. Panel publikuje komendy kotła, Discovery, diagnostykę, availability i obsługuje komendę restartu.

## Wymagania

Potrzebny jest działający broker MQTT z włączoną integracją MQTT w Home Assistant. Panel i Home Assistant muszą łączyć się z tym samym brokerem. Dane Wi-Fi i MQTT dla ESP są lokalne w `include/secrets.h`; plik jest ignorowany przez Git. Użyj szablonu [include/secrets.example.h](../include/secrets.example.h) i nie umieszczaj prawdziwych danych logowania w repozytorium.

## Logiczne sensory zewnętrzne

Firmware korzysta ze stabilnych logicznych encji:

- `sensor.temperatura_zewnetrzna`
- `sensor.cisnienie_zewnetrzne`

Fizyczne źródło może być później zmienione bez zmiany firmware. Może nim być encja weather, fizyczny czujnik, template sensor albo integracja internetowa. Konkretne źródło zależy od instalacji.

## Temperatura i ciśnienie

Temperatura jest publikowana na `ha/shared/outside_temperature/state` jako liczba bez jednostki, np. `13.3`. Ciśnienie jest publikowane na `ha/shared/outside_pressure/state` jako liczba w hPa bez jednostki, np. `1023`. `unknown`, `unavailable`, pusty lub nieparsowalny payload jest nieważny; `0.0` jest poprawną temperaturą.

Panel używa MQTT HA jako podstawowego źródła. Po 15 minutach bez poprawnej wiadomości przełącza daną wartość na Open-Meteo (`current.temperature_2m` lub `pressure_msl`). Gwiazdka przy wartości na HOME oznacza fallback, a brak ciśnienia jest pokazywany jako `--- hPa`. Temperatura i ciśnienie wybierają źródło niezależnie.

Retained message nie zastępuje heartbeat. Publikuj oba topici przy zmianie stanu, przy starcie Home Assistant i co 5 minut. Pięć minut jest krótsze niż firmware'owy timeout 15 minut.

### Automatyzacja temperatury

```yaml
alias: MQTT - temperatura zewnętrzna
description: Publikuje wspólną temperaturę zewnętrzną dla sterowników
triggers:
  - trigger: state
    entity_id:
      - sensor.temperatura_zewnetrzna
  - trigger: homeassistant
    event: start
  - trigger: time_pattern
    minutes: "/5"
actions:
  - action: mqtt.publish
    data:
      topic: ha/shared/outside_temperature/state
      payload: >
        {% set v = states('sensor.temperatura_zewnetrzna') %}
        {% if v not in ['unknown', 'unavailable', 'none', ''] %}
          {{ v }}
        {% else %}
          unavailable
        {% endif %}
      qos: 1
      retain: true
mode: restart
```

### Automatyzacja ciśnienia

```yaml
alias: MQTT - ciśnienie zewnętrzne
description: Publikuje wspólne ciśnienie zewnętrzne dla sterowników
triggers:
  - trigger: state
    entity_id:
      - sensor.cisnienie_zewnetrzne
  - trigger: homeassistant
    event: start
  - trigger: time_pattern
    minutes: "/5"
actions:
  - action: mqtt.publish
    data:
      topic: ha/shared/outside_pressure/state
      payload: >
        {% set v = states('sensor.cisnienie_zewnetrzne') %}
        {% if v not in ['unknown', 'unavailable', 'none', ''] %}
          {{ v }}
        {% else %}
          unavailable
        {% endif %}
      qos: 1
      retain: true
mode: restart
```

## Kocioł: stany i komendy

Panel subskrybuje `kuchnia-panel/ha/state/climate`. Payload jest JSON-em:

```json
{
  "hvac_mode": "heat",
  "preset_mode": "comfort",
  "hvac_action": "heating",
  "target_temperature": 21.5,
  "current_temperature": 20.8
}
```

Panel subskrybuje też `kuchnia-panel/ha/state/boiler_power` z payloadem `ON` albo `OFF`. `hvac_mode` obsługuje `heat` i `off`, `preset_mode` obsługuje `comfort`, `sleep` i `manual`, a `hvac_action=heating` oznacza aktywne grzanie i płomień. MQTT jest źródłem prawdy.

Panel publikuje komendy:

| Topic | Payload |
|---|---|
| `kuchnia-panel/ha/command/climate/hvac_mode` | `heat` / `off` |
| `kuchnia-panel/ha/command/climate/preset_mode` | `comfort` / `sleep` |
| `kuchnia-panel/ha/command/climate/temperature` | np. `21.5`, zakres 15.0–25.0°C |
| `kuchnia-panel/ha/command/boiler_power` | `ON` / `OFF` |

Ekran temperatury może pokazać lokalną zmianę natychmiast, ale późniejszy stan MQTT nadpisuje podgląd. Po połączeniu panel publikuje `request` na `kuchnia-panel/ha/snapshot/request`, aby poprosić o aktualny stan kotła. Instalacja HA/brokera musi mieć istniejącą logikę odpowiadającą na ten kontrakt; repozytorium nie definiuje automatyzacji snapshotu.

## MQTT Discovery i diagnostyka

Panel publikuje retained Discovery w namespace `nexapanel-mini` dla urządzenia `NexaPanel Mini`. Nie trzeba tworzyć tych encji ręcznie:

- `homeassistant/sensor/nexapanel-mini/rssi/config`
- `homeassistant/sensor/nexapanel-mini/uptime/config`
- `homeassistant/sensor/nexapanel-mini/firmware/config`
- `homeassistant/button/nexapanel-mini/restart/config`

Stany i komenda restartu:

- `nexapanel-mini/status`: `online` / `offline` przez MQTT LWT, retained,
- `nexapanel-mini/rssi/state`,
- `nexapanel-mini/uptime/state`,
- `nexapanel-mini/firmware/state` — retained, aktualnie `1.0.0`,
- `nexapanel-mini/restart/set` z payloadem `PRESS`.

Discovery jest publikowane po połączeniu i reconnect. RSSI oraz uptime są publikowane po połączeniu i co 30 sekund. Availability pokazuje rzeczywistą dostępność panelu.

Temperatura i ciśnienie zewnętrzne nie są encjami Discovery tworzonymi przez panel; są wejściami danych używanymi przez HOME.

## Troubleshooting

- Gwiazdka przy temperaturze: sprawdź MQTT, `ha/shared/outside_temperature/state`, heartbeat i stan `sensor.temperatura_zewnetrzna`.
- Gwiazdka przy ciśnieniu: sprawdź `ha/shared/outside_pressure/state`, heartbeat i `sensor.cisnienie_zewnetrzne`.
- Panel unavailable: sprawdź `nexapanel-mini/status`, broker, Wi-Fi i log panelu.
- Brak Discovery: sprawdź aktywną integrację MQTT w HA, topici `homeassistant/.../config` i połączenie panelu z brokerem.
- Niewłaściwa wersja: sprawdź `nexapanel-mini/firmware/state` i właściwy build.
- Brak danych kotła po starcie: sprawdź `kuchnia-panel/ha/snapshot/request`, istniejącą logikę odpowiedzi oraz topici stanu kotła.
