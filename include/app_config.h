#pragma once

namespace AppConfig {
constexpr const char* DEVICE_NAME = "NexaPanelMini";
constexpr const char* HOSTNAME = "nexapanel-mini";

// Uzupełnij lokalnie przed testem NTP. Puste SSID pozostawia Wi-Fi wyłączone.
constexpr const char* WIFI_SSID = "";
constexpr const char* WIFI_PASSWORD = "";

constexpr unsigned long UI_REFRESH_MS = 250;
constexpr unsigned long UI_PRESSED_MS = 120;
constexpr unsigned long NTP_RESYNC_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr unsigned long WEATHER_REFRESH_MS = 15UL * 60UL * 1000UL;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr unsigned long WIFI_RECONNECT_MIN_MS = 1000;
constexpr unsigned long WIFI_RECONNECT_MAX_MS = 30000;
constexpr unsigned long DIAGNOSTIC_INTERVAL_MS = 30000;

constexpr const char* TIMEZONE_RULE = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* NTP_SERVER_PRIMARY = "pool.ntp.org";
constexpr const char* NTP_SERVER_SECONDARY = "time.google.com";
constexpr const char* NTP_SERVER_TERTIARY = "time.cloudflare.com";

// Kalibracja kontrolera XPT2046 dla LCD rotation=0.
constexpr int16_t TOUCH_X_MIN = 400;
constexpr int16_t TOUCH_X_MAX = 3550;
constexpr int16_t TOUCH_Y_MIN = 550;
constexpr int16_t TOUCH_Y_MAX = 3700;
}
