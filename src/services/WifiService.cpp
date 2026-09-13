#include "services/WifiService.h"
#include <ESP8266WiFi.h>
#include "app_config.h"

void WifiService::begin() {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(AppConfig::HOSTNAME);
    reconnectDelayMs_ = AppConfig::WIFI_RECONNECT_MIN_MS;

    if (AppConfig::WIFI_SSID[0] == '\0') {
        phase_ = Phase::DISABLED;
        Serial.println("WIFI DISABLED: ustaw WIFI_SSID w app_config.h");
        return;
    }

    startConnection();
}

void WifiService::update(AppState& state) {
    if (phase_ == Phase::DISABLED) return;

    const uint32_t now = millis();
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && phase_ != Phase::CONNECTED) {
        phase_ = Phase::CONNECTED;
        reconnectDelayMs_ = AppConfig::WIFI_RECONNECT_MIN_MS;
        state.wifiConnected = true;
        state.wifiRssi = WiFi.RSSI();
        state.ipAddress = WiFi.localIP().toString();
        Serial.printf("WIFI CONNECTED: IP=%s RSSI=%d dBm\n",
                      state.ipAddress.c_str(), state.wifiRssi);
        return;
    }

    if (phase_ == Phase::CONNECTED) {
        if (connected) {
            state.wifiRssi = WiFi.RSSI();
            return;
        }
        state.wifiConnected = false;
        state.wifiRssi = -127;
        state.ipAddress = "0.0.0.0";
        Serial.println("WIFI DISCONNECTED");
        scheduleRetry();
        return;
    }

    if (phase_ == Phase::CONNECTING &&
        now - phaseStartedMs_ >= AppConfig::WIFI_CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(false);
        Serial.println("WIFI CONNECT TIMEOUT");
        scheduleRetry();
    } else if (phase_ == Phase::WAIT_RETRY &&
               now - phaseStartedMs_ >= reconnectDelayMs_) {
        startConnection();
    }
}

void WifiService::startConnection() {
    phase_ = Phase::CONNECTING;
    phaseStartedMs_ = millis();
    Serial.printf("WIFI CONNECTING: %s\n", AppConfig::WIFI_SSID);
    WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);
}

void WifiService::scheduleRetry() {
    phase_ = Phase::WAIT_RETRY;
    phaseStartedMs_ = millis();
    Serial.printf("WIFI RETRY: %lu ms\n",
                  static_cast<unsigned long>(reconnectDelayMs_));
    reconnectDelayMs_ = min(reconnectDelayMs_ * 2UL,
                            AppConfig::WIFI_RECONNECT_MAX_MS);
}
