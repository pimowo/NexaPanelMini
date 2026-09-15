#pragma once
#include <MQTT.h>
#include <ESP8266WiFi.h>
#include <limits.h>
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
    void parseOutsideTemperature(String payload);
    void parsePanelRestartCommand(String payload);
    bool publishCommand(const char* topic, const char* payload);
    bool buildTopics();
    bool buildBoilerTopic(char* target, size_t size, const char* suffix);
    bool buildPanelTopic(char* target, size_t size, const char* suffix);
    bool buildDiscoveryTopic(char* target, size_t size,
                             const char* component,
                             const char* objectId);
    bool publishPanelAvailabilityOnline();
    bool publishPanelDiscovery();
    void publishPanelTelemetry(bool force);
    void scheduleRetry();

    struct Topics {
        char climate[96]{};
        char boilerPower[96]{};
        char snapshotRequest[96]{};
        char commandHvacMode[96]{};
        char commandPresetMode[96]{};
        char commandTemperature[96]{};
        char commandBoilerPower[96]{};
        char panelAvailability[96]{};
        char panelRssiState[96]{};
        char panelUptimeState[96]{};
        char panelFirmwareState[96]{};
        char panelOutsideTemperatureState[96]{};
        char panelRestartSet[96]{};
        char discoveryRssi[128]{};
        char discoveryUptime[128]{};
        char discoveryFirmware[128]{};
        char discoveryRestart[128]{};
    } topics_;

    AppState* state_ = nullptr;
    WiFiClient networkClient_;
    MQTTClient mqttClient_{768};
    Phase phase_ = Phase::IDLE;
    uint32_t retryStartedMs_ = 0;
    uint32_t retryDelayMs_ = 0;
    uint32_t nextPanelTelemetryMs_ = 0;
    uint8_t backoffStep_ = 0;
    bool wifiWasConnected_ = false;
    bool restartRequested_ = false;
    bool outsideTempStaleLogged_ = false;
    bool outsideTempHadValidSample_ = false;
    int lastPublishedRssi_ = INT_MIN;
    uint32_t lastPublishedUptimeS_ = UINT32_MAX;
};
