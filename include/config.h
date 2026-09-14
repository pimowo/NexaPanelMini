#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID_VALUE
#define WIFI_SSID_VALUE ""
#endif

#ifndef WIFI_PASSWORD_VALUE
#define WIFI_PASSWORD_VALUE ""
#endif

#ifndef MQTT_USER_VALUE
#define MQTT_USER_VALUE ""
#endif

#ifndef MQTT_PASSWORD_VALUE
#define MQTT_PASSWORD_VALUE ""
#endif

namespace AppConfig {

// ============================================================
// Identyfikacja urządzenia
// ============================================================
constexpr const char* DEVICE_NAME = "NexaPanelMini";
constexpr const char* HOSTNAME = "nexapanel-mini";

// ============================================================
// Wi-Fi
// ============================================================

constexpr const char* WIFI_SSID = WIFI_SSID_VALUE;
constexpr const char* WIFI_PASSWORD = WIFI_PASSWORD_VALUE;

// ============================================================
// Broker MQTT
// ============================================================

constexpr const char* MQTT_HOST = "192.168.1.14";
constexpr uint16_t MQTT_PORT = 1883;
constexpr const char* MQTT_USER = MQTT_USER_VALUE;
constexpr const char* MQTT_PASSWORD = MQTT_PASSWORD_VALUE;
constexpr const char* MQTT_CLIENT_ID = "nexapanel-mini";
constexpr uint16_t MQTT_KEEPALIVE_S = 15;

// ============================================================
// MQTT - komunikacja kotła
// ============================================================

// Historyczna wartość jest zachowana celowo dla kompatybilności istniejącego
// brokera i automatyzacji kotła.
constexpr const char* BOILER_MQTT_BASE_TOPIC = "kuchnia-panel";

// ============================================================
// MQTT - NexaPanel Mini / Home Assistant Discovery
// ============================================================

// Osobna przestrzeń topiców panelu (diagnostyka + discovery), bez mieszania z
// topicami kotła.
constexpr const char* PANEL_MQTT_BASE_TOPIC = "nexapanel-mini";
constexpr const char* PANEL_MQTT_DISCOVERY_PREFIX = "homeassistant";
constexpr const char* PANEL_DEVICE_IDENTIFIER = "nexapanel-mini";
constexpr const char* PANEL_DEVICE_NAME = "NexaPanel Mini";
constexpr const char* PANEL_DEVICE_MANUFACTURER = "DIY / pimowo";
constexpr const char* PANEL_DEVICE_MODEL = "NexaPanel Mini ESP8266";

// Częstotliwość publikacji telemetrii panelu do MQTT (ms).
constexpr unsigned long PANEL_MQTT_DIAGNOSTIC_PUBLISH_MS = 30000UL;

// ============================================================
// yoRadio
// ============================================================

constexpr const char* YORADIO_HOST = "192.168.1.114";
constexpr uint16_t YORADIO_PORT = 80;
constexpr const char* YORADIO_PATH = "/ws";

// ============================================================
// Pogoda Open-Meteo
// ============================================================

constexpr const char* WEATHER_LATITUDE = "51.33546";
constexpr const char* WEATHER_LONGITUDE = "16.63478";

// ============================================================
// Interfejs użytkownika
// ============================================================

constexpr unsigned long UI_HOME_TIMEOUT_MS = 30000UL;
constexpr unsigned long UI_VOLUME_HOLD_INITIAL_MS = 500UL;
constexpr unsigned long UI_VOLUME_HOLD_REPEAT_MS = 225UL;

// ============================================================
// Timeouty i reconnect
// ============================================================

constexpr unsigned long NTP_RESYNC_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr unsigned long WEATHER_REFRESH_MS = 15UL * 60UL * 1000UL;
constexpr unsigned long WEATHER_RETRY_MS = 60000UL;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;
constexpr unsigned long WIFI_RECONNECT_MIN_MS = 1000UL;
constexpr unsigned long WIFI_RECONNECT_MAX_MS = 30000UL;
constexpr unsigned long YORADIO_CONNECT_TIMEOUT_MS = 5000UL;
constexpr unsigned long YORADIO_FIRST_DATA_TIMEOUT_MS = 5000UL;
constexpr unsigned long YORADIO_SILENCE_TIMEOUT_MS = 60000UL;
constexpr unsigned long YORADIO_RECONNECT_MIN_MS = 1000UL;
constexpr unsigned long YORADIO_RECONNECT_MAX_MS = 30000UL;
constexpr unsigned long MQTT_CONNECT_TIMEOUT_MS = 1000UL;
constexpr unsigned long MQTT_RECONNECT_MIN_MS = 1000UL;
constexpr unsigned long MQTT_RECONNECT_MAX_MS = 30000UL;
constexpr unsigned long DIAGNOSTIC_INTERVAL_MS = 30000UL;

// ============================================================
// NTP i strefa czasowa
// ============================================================

constexpr const char* TIMEZONE_RULE = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* NTP_SERVER_PRIMARY = "pool.ntp.org";
constexpr const char* NTP_SERVER_SECONDARY = "time.google.com";
constexpr const char* NTP_SERVER_TERTIARY = "time.cloudflare.com";

// ============================================================
// Kalibracja dotyku
// ============================================================

// Kalibracja kontrolera XPT2046 dla LCD rotation=0. Zmieniaj tylko po
// rzeczywistym pomiarze osi dotyku na docelowym panelu.
constexpr int16_t TOUCH_X_MIN = 400;
constexpr int16_t TOUCH_X_MAX = 3550;
constexpr int16_t TOUCH_Y_MIN = 550;
constexpr int16_t TOUCH_Y_MAX = 3700;
}