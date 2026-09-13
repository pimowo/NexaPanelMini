#pragma once

#include <Arduino.h>

// ============================================================================
// KONFIGURACJA PANELU KUCHENNEGO
// ============================================================================

// Wartości użytkownika są domyślnymi wartościami RuntimeConfig.
// Stałe techniczne projektu HMI znajdują się osobno w NextionIds.h.
namespace Config {

// IDENTYFIKACJA
namespace Identity {
constexpr char DEVICE_NAME[] = "Panel kuchenny";
constexpr char HOSTNAME[] = "kuchnia-panel";
constexpr char MQTT_BASE_TOPIC[] = "kuchnia-panel";
constexpr char HA_DISCOVERY_PREFIX[] = "homeassistant";
constexpr char DEVICE_ID[] = "kuchnia_panel";
constexpr char MANUFACTURER[] = "pimowo";
constexpr char MODEL[] = "ESP32-S3 + Nextion";
}  // namespace Identity

// WI-FI
namespace Wifi {
constexpr char WIFI_SSID[] = "pimowo";
constexpr char WIFI_PASSWORD[] = "ckH59LRZQzCDQFiUgj";
constexpr const char* WIFI_HOSTNAME = Identity::HOSTNAME;

// Domyślnie używany jest DHCP. Pola adresów są używane tylko po jego wyłączeniu.
constexpr bool DHCP_ENABLED = true;
constexpr char STATIC_IP[] = "";
constexpr char GATEWAY[] = "";
constexpr char SUBNET[] = "";
constexpr char DNS1[] = "";
constexpr char DNS2[] = "";
constexpr uint32_t RECONNECT_MIN_MS = 1000;
constexpr uint32_t RECONNECT_MAX_MS = 30000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t STATE_REFRESH_INTERVAL_MS = 5000;
}  // namespace Wifi

// MQTT
namespace Mqtt {
constexpr char BROKER_HOST[] = "192.168.1.14";
constexpr uint16_t BROKER_PORT = 1883;
constexpr char USERNAME[] = "piotrek";
constexpr char PASSWORD[] = "pm1981";
constexpr char CLIENT_ID[] = "kuchnia-panel";
constexpr const char* BASE_TOPIC = Identity::MQTT_BASE_TOPIC;
constexpr const char* DISCOVERY_PREFIX = Identity::HA_DISCOVERY_PREFIX;
constexpr uint16_t KEEPALIVE_SECONDS = 30;
constexpr uint32_t CONNECT_TIMEOUT_MS = 1000;
constexpr uint32_t OPERATION_TIMEOUT_MS = 1000;
constexpr uint32_t RECONNECT_MIN_MS = 1000;
constexpr uint32_t RECONNECT_MAX_MS = 30000;
constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 60000;
constexpr int32_t RSSI_CHANGE_THRESHOLD_DB = 3;
constexpr size_t BUFFER_SIZE = 2048;
constexpr size_t TOPIC_BUFFER_SIZE = 128;
constexpr size_t DISCOVERY_JSON_SIZE = 1024;
constexpr bool DISCOVERY_ENABLED = true;
}  // namespace Mqtt

// DANE HOME ASSISTANT
namespace HomeAssistant {
constexpr uint32_t SNAPSHOT_RETRY_INTERVAL_MS = 10000;
constexpr size_t CLIMATE_RAW_TEXT_MAX_BYTES = 24;
constexpr size_t CALENDAR_ENTRY_MAX_BYTES = 96;
// Parametry przewijania wpisów kalendarza w istniejących polach 192 px.
constexpr uint16_t CALENDAR_VISIBLE_WIDTH_PX = 190;
constexpr uint8_t CALENDAR_COMPONENT_MAX_CHARS = 32;
constexpr uint32_t CALENDAR_MARQUEE_START_PAUSE_MS = 2000;
constexpr uint32_t CALENDAR_MARQUEE_STEP_MS = 190;
constexpr uint32_t CALENDAR_MARQUEE_END_PAUSE_MS = 1000;
constexpr uint8_t CALENDAR_MARQUEE_MAX_UPDATES_PER_LOOP = 2;
constexpr size_t WASTE_TEXT_MAX_BYTES = 32;
// Maksymalna długość poprawnego terminu odbioru wyświetlanego na START.
constexpr size_t WASTE_UI_MAX_CHARS = 10;
constexpr float WEATHER_TEMPERATURE_MIN_C = -99.9f;
constexpr float WEATHER_TEMPERATURE_MAX_C = 99.9f;
constexpr size_t CLIMATE_JSON_SIZE = 512;
constexpr size_t CALENDAR_JSON_SIZE = 1536;
constexpr size_t WASTE_JSON_SIZE = 512;
}  // namespace HomeAssistant

// YORADIO
namespace YoRadio {
constexpr char HOST[] = "192.168.1.101";
constexpr uint16_t PORT = 80;
constexpr char PATH[] = "/ws";
constexpr uint32_t CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t FIRST_DATA_TIMEOUT_MS = 5000;
constexpr uint32_t RECONNECT_MIN_MS = 1000;
constexpr uint32_t RECONNECT_MAX_MS = 30000;
constexpr int VOLUME_MIN = 0;
constexpr int VOLUME_MAX = 100;
constexpr int BITRATE_MIN = 1;
constexpr int BITRATE_MAX = 9999;
constexpr size_t UI_FORMAT_MAX_CHARS = 5;
constexpr size_t UI_MEDIA_TEXT_MAX_CHARS = 64;
}  // namespace YoRadio

// NEXTION
namespace Nextion {
constexpr int8_t TX_PIN = 4;
constexpr int8_t RX_PIN = 5;
constexpr uint32_t BAUD_RATE = 115200;
constexpr size_t COMMAND_QUEUE_SIZE = 32;
constexpr size_t MAX_COMMAND_LENGTH = 128;
constexpr size_t RX_FRAME_BUFFER_SIZE = 64;
constexpr size_t RX_BYTES_PER_LOOP = 64;
constexpr uint32_t RX_FRAME_TIMEOUT_MS = 250;
constexpr uint32_t STARTUP_SYNC_SETTLE_MS = 500;
constexpr uint32_t STARTUP_SYNC_RETRY_MS = 400;
constexpr uint32_t BOOT_PAGE_SETTLE_MS = 100;
constexpr uint32_t LIVENESS_CHECK_INTERVAL_MS = 5000;
constexpr uint32_t LIVENESS_TIMEOUT_MS = 15000;
constexpr uint32_t COMMAND_INTERVAL_MS = 20;
}  // namespace Nextion

// AKTUALIZACJA HMI NEXTION PRZEZ PRZEGLĄDARKĘ
namespace NextionUpdate {
constexpr bool ENABLED = true;
constexpr uint16_t WEB_PORT = 80;
// Krótkie czasy techniczne dotyczą wyłącznie przygotowania strony UPDATE.
constexpr uint32_t UPDATE_PAGE_SETTLE_MS = 100;
constexpr uint32_t UI_RX_DRAIN_MS = 50;
// Oficjalny protokół uploadu Nextion wymienia 921600 jako prawidłową wartość.
constexpr uint32_t UPLOAD_BAUD = 921600;
constexpr uint32_t ACK_TIMEOUT_MS = 1000;
// NX4832T035 ma według specyfikacji producenta 16 MB pamięci Flash.
constexpr size_t MAX_FILE_SIZE = 16U * 1024U * 1024U;
constexpr size_t CHUNK_SIZE = 4096;

// Limity techniczne nieblokującej obsługi HTTP i powrotu do zwykłego UART.
constexpr size_t HTTP_HEADER_MAX_SIZE = 1024;
constexpr size_t FILE_NAME_MAX_SIZE = 96;
constexpr size_t HTTP_READ_BYTES_PER_LOOP = 1024;
constexpr size_t JSON_BODY_MAX_SIZE = 4096;
constexpr uint32_t HTTP_HEADER_TIMEOUT_MS = 5000;
constexpr uint32_t HTTP_BODY_TIMEOUT_MS = 15000;
constexpr uint32_t API_RESTART_DELAY_MS = 250;
constexpr uint32_t FIRMWARE_RESTART_DELAY_MS = 1000;
constexpr uint32_t NEXTION_CONNECT_TIMEOUT_MS = 1500;
constexpr uint32_t BAUD_SWITCH_DELAY_MS = 50;
constexpr uint32_t NEXTION_REBOOT_WAIT_MS = 50;
constexpr uint32_t NORMAL_BAUD_SETTLE_MS = 3000;
constexpr uint32_t RECOVERY_TIMEOUT_MS = 3000;
constexpr uint32_t SUCCESS_RESTART_DELAY_MS = 1500;
}  // namespace NextionUpdate

// NTP I STREFA CZASOWA
namespace Ntp {
constexpr char TIMEZONE_NAME[] = "Europe/Warsaw";
constexpr char TIMEZONE_RULE[] = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr char SERVER_PRIMARY[] = "pool.ntp.org";
constexpr char SERVER_SECONDARY[] = "time.google.com";
constexpr char SERVER_TERTIARY[] = "time.cloudflare.com";
constexpr uint32_t SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t VALIDITY_CHECK_INTERVAL_MS = 1000;
constexpr int MIN_VALID_YEAR = 2024;
}  // namespace Ntp

// INTERFEJS UŻYTKOWNIKA
namespace Ui {
constexpr uint32_t UI_PAGE_TIMEOUT_MS = 10000;
constexpr uint32_t CLOCK_UPDATE_INTERVAL_MS = 1000;
constexpr uint32_t HOLD_DELAY_MS = 500;
constexpr uint32_t VOLUME_REPEAT_MS = 250;
constexpr uint32_t BOOT_MAX_WAIT_MS = 30000;
constexpr uint32_t BOOT_SPLASH_DOT_INTERVAL_MS = 350;
}  // namespace Ui

// KOCIOŁ
namespace Boiler {
constexpr float TARGET_MIN_C = 15.0F;
constexpr float TARGET_MAX_C = 25.0F;
constexpr float TARGET_STEP_C = 0.1F;
constexpr uint32_t TEMP_HOLD_DELAY_MS = 500;
constexpr uint32_t TEMP_REPEAT_MS = 250;
constexpr uint32_t MQTT_CONFIRM_TIMEOUT_MS = 2500;
}  // namespace Boiler

// LOGOWANIE
namespace Logging {
enum class Level : uint8_t {
  ERROR,
  WARNING,
  INFO,
  DEBUG,
};

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint32_t USB_CDC_READY_TIMEOUT_MS = 2000;
constexpr uint32_t USB_CDC_POLL_INTERVAL_MS = 10;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;
constexpr uint32_t HEALTH_SAMPLE_INTERVAL_MS = 5000;
constexpr uint32_t LOOP_STACK_WARNING_BYTES = 2048;
constexpr bool SERIAL0_DIAGNOSTIC_FALLBACK = true;
constexpr Level LEVEL = Level::INFO;
constexpr bool DEBUG_WIFI = false;
constexpr bool DEBUG_MQTT = false;
constexpr bool DEBUG_WEBSOCKET = false;
constexpr bool DEBUG_WEBSOCKET_RAW = false;
constexpr bool DEBUG_NEXTION_RX = false;
constexpr bool DEBUG_NEXTION_TX = false;
constexpr bool DEBUG_HTTP_STACK = false;
}  // namespace Logging

}  // namespace Config
