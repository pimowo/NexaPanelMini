#pragma once
#include <MQTT.h>
#include <ESP8266WiFi.h>
#include "core/AppState.h"

class BoilerService {
public:
    void begin(AppState& state);
    void update(AppState& state);
    bool setHvacEnabled(bool enabled);
    bool setComfortMode(bool comfort);
    bool setTargetTemperature(float temperature);
    bool setPower(bool enabled);

private:
    enum class Phase : uint8_t {
        DISABLED,
        IDLE,
        CONNECTED,
        WAIT_RETRY
    };

    void attemptConnection();
    void handleConnected();
    void handleDisconnected(bool retry);
    void handleMessage(String& topic, String& payload);
    void parseClimate(const String& payload);
    void parseBoilerPower(String payload);
    bool publishCommand(const char* topic, const char* payload);
    bool buildTopics();
    bool buildTopic(char* target, size_t size, const char* suffix);
    void scheduleRetry();

    struct Topics {
        char climate[96]{};
        char boilerPower[96]{};
        char snapshotRequest[96]{};
        char commandHvacMode[96]{};
        char commandPresetMode[96]{};
        char commandTemperature[96]{};
        char commandBoilerPower[96]{};
    } topics_;

    AppState* state_ = nullptr;
    WiFiClient networkClient_;
    MQTTClient mqttClient_{768};
    Phase phase_ = Phase::IDLE;
    uint32_t retryStartedMs_ = 0;
    uint32_t retryDelayMs_ = 0;
    uint8_t backoffStep_ = 0;
    bool wifiWasConnected_ = false;
};
