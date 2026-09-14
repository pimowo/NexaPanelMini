#include "services/BoilerService.h"
#include <ArduinoJson.h>
#include <math.h>
#include "config.h"

namespace {
constexpr char CLIMATE_SUFFIX[] = "/ha/state/climate";
constexpr char BOILER_POWER_SUFFIX[] = "/ha/state/boiler_power";
constexpr char SNAPSHOT_SUFFIX[] = "/ha/snapshot/request";
constexpr char HVAC_COMMAND_SUFFIX[] = "/ha/command/climate/hvac_mode";
constexpr char PRESET_COMMAND_SUFFIX[] = "/ha/command/climate/preset_mode";
constexpr char TEMPERATURE_COMMAND_SUFFIX[] =
    "/ha/command/climate/temperature";
constexpr char POWER_COMMAND_SUFFIX[] = "/ha/command/boiler_power";
constexpr float TARGET_MIN_C = 15.0F;
constexpr float TARGET_MAX_C = 25.0F;

bool differentFloat(float first, float second) {
    return isnan(first) != isnan(second) ||
           (!isnan(first) && fabsf(first - second) >= 0.01F);
}

bool readNumber(JsonVariantConst value, float& result) {
    if (!(value.is<float>() || value.is<double>() || value.is<int>() ||
          value.is<long>() || value.is<unsigned int>() ||
          value.is<unsigned long>())) {
        return false;
    }
    result = value.as<float>();
    return isfinite(result);
}
}

void BoilerService::begin(AppState& state) {
    state_ = &state;
    retryDelayMs_ = AppConfig::MQTT_RECONNECT_MIN_MS;
    if (AppConfig::MQTT_HOST[0] == '\0' ||
        AppConfig::MQTT_CLIENT_ID[0] == '\0' ||
        AppConfig::MQTT_BASE_TOPIC[0] == '\0' || !buildTopics()) {
        phase_ = Phase::DISABLED;
        Serial.println("MQTT DISABLED: brak lub błędna konfiguracja");
        return;
    }

    networkClient_.setTimeout(AppConfig::MQTT_CONNECT_TIMEOUT_MS);
    mqttClient_.begin(AppConfig::MQTT_HOST, AppConfig::MQTT_PORT,
                      networkClient_);
    mqttClient_.setOptions(AppConfig::MQTT_KEEPALIVE_S, true,
                           AppConfig::MQTT_CONNECT_TIMEOUT_MS);
    mqttClient_.onMessage([this](String& topic, String& payload) {
        handleMessage(topic, payload);
    });
}

void BoilerService::update(AppState& state) {
    state_ = &state;
    if (phase_ == Phase::DISABLED) return;

    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (!wifiConnected) {
        if (wifiWasConnected_ || phase_ != Phase::IDLE) {
            networkClient_.stop();
            wifiWasConnected_ = false;
            phase_ = Phase::IDLE;
            if (state_->boilerOnline) {
                state_->boilerOnline = false;
                ++state_->boilerRevision;
            }
        }
        return;
    }

    if (!wifiWasConnected_) {
        wifiWasConnected_ = true;
        backoffStep_ = 0;
        phase_ = Phase::IDLE;
    }
    if (phase_ == Phase::WAIT_RETRY) {
        if (millis() - retryStartedMs_ >= retryDelayMs_) attemptConnection();
        return;
    }
    if (phase_ == Phase::IDLE) {
        attemptConnection();
        return;
    }
    if (!mqttClient_.connected() || !mqttClient_.loop()) {
        handleDisconnected(true);
    }
}

bool BoilerService::setHvacEnabled(bool enabled) {
    return publishCommand(topics_.commandHvacMode, enabled ? "heat" : "off");
}

bool BoilerService::setComfortMode(bool comfort) {
    return publishCommand(topics_.commandPresetMode,
                          comfort ? "comfort" : "sleep");
}

bool BoilerService::setTargetTemperature(float temperature) {
    if (!isfinite(temperature) || temperature < TARGET_MIN_C ||
        temperature > TARGET_MAX_C) {
        return false;
    }
    const float rounded = roundf(temperature * 10.0F) / 10.0F;
    char payload[16]{};
    snprintf(payload, sizeof(payload), "%.1f", rounded);
    return publishCommand(topics_.commandTemperature, payload);
}

bool BoilerService::setPower(bool enabled) {
    return publishCommand(topics_.commandBoilerPower, enabled ? "ON" : "OFF");
}

void BoilerService::attemptConnection() {
    networkClient_.stop();
    Serial.println("MQTT CONNECTING");
    const bool connected = AppConfig::MQTT_USER[0] == '\0'
        ? mqttClient_.connect(AppConfig::MQTT_CLIENT_ID)
        : mqttClient_.connect(AppConfig::MQTT_CLIENT_ID,
                              AppConfig::MQTT_USER,
                              AppConfig::MQTT_PASSWORD);
    if (connected) handleConnected();
    else handleDisconnected(true);
}

void BoilerService::handleConnected() {
    const bool subscribed =
        mqttClient_.subscribe(topics_.climate, 1) &&
        mqttClient_.subscribe(topics_.boilerPower, 1);
    if (!subscribed ||
        !mqttClient_.publish(topics_.snapshotRequest, "request", false, 0)) {
        handleDisconnected(true);
        return;
    }
    phase_ = Phase::CONNECTED;
    backoffStep_ = 0;
    if (!state_->boilerOnline) {
        state_->boilerOnline = true;
        ++state_->boilerRevision;
    }
    Serial.println("MQTT CONNECTED");
    Serial.println("MQTT SNAPSHOT requested");
}

void BoilerService::handleDisconnected(bool retry) {
    const bool shouldLog = phase_ == Phase::CONNECTED ||
                           (state_ && state_->boilerOnline);
    networkClient_.stop();
    phase_ = Phase::IDLE;
    if (state_ && state_->boilerOnline) {
        state_->boilerOnline = false;
        ++state_->boilerRevision;
    }
    if (shouldLog) Serial.println("MQTT DISCONNECTED");
    if (retry && WiFi.status() == WL_CONNECTED) scheduleRetry();
}

void BoilerService::handleMessage(String& topic, String& payload) {
    if (topic == topics_.climate) parseClimate(payload);
    else if (topic == topics_.boilerPower) parseBoilerPower(payload);
}

void BoilerService::parseClimate(const String& payload) {
    StaticJsonDocument<512> document;
    if (deserializeJson(document, payload) || !document.is<JsonObject>()) {
        Serial.println("MQTT CLIMATE parse error");
        return;
    }
    const JsonObjectConst root = document.as<JsonObjectConst>();
    bool changed = false;

    const char* hvacMode = root["hvac_mode"] | "";
    if (hvacMode[0]) {
        String mode = hvacMode;
        mode.trim();
        const bool heat = mode.equalsIgnoreCase("heat");
        if ((heat || mode.equalsIgnoreCase("off")) &&
            state_->boilerHvacHeat != heat) {
            state_->boilerHvacHeat = heat;
            changed = true;
        }
    }

    const char* presetMode = root["preset_mode"] | "";
    if (presetMode[0]) {
        String preset = presetMode;
        preset.trim();
        const bool comfort = preset.equalsIgnoreCase("comfort");
        if ((comfort || preset.equalsIgnoreCase("sleep")) &&
            state_->boilerComfortMode != comfort) {
            state_->boilerComfortMode = comfort;
            changed = true;
        }
    }

    const char* hvacAction = root["hvac_action"] | "";
    if (hvacAction[0]) {
        String action = hvacAction;
        action.trim();
        const bool heating = action.equalsIgnoreCase("heating");
        if ((heating || action.equalsIgnoreCase("idle") ||
             action.equalsIgnoreCase("off")) &&
            state_->boilerEnabled != heating) {
            state_->boilerEnabled = heating;
            changed = true;
        }
    }

    float temperature = 0.0F;
    if (readNumber(root["target_temperature"], temperature) &&
        temperature >= TARGET_MIN_C && temperature <= TARGET_MAX_C &&
        differentFloat(state_->boilerTargetTemp, temperature)) {
        state_->boilerTargetTemp = temperature;
        changed = true;
    }
    if (readNumber(root["current_temperature"], temperature) &&
        differentFloat(state_->boilerCurrentTemp, temperature)) {
        state_->boilerCurrentTemp = temperature;
        changed = true;
    }
    if (changed) ++state_->boilerRevision;
}

void BoilerService::parseBoilerPower(String payload) {
    payload.trim();
    const bool recognized = payload.equalsIgnoreCase("ON") ||
                            payload.equalsIgnoreCase("OFF");
    if (!recognized) return;
    const bool enabled = payload.equalsIgnoreCase("ON");
    if (state_->boilerPowerOn != enabled) {
        state_->boilerPowerOn = enabled;
        ++state_->boilerRevision;
    }
}

bool BoilerService::publishCommand(const char* topic, const char* payload) {
    return phase_ == Phase::CONNECTED && mqttClient_.connected() &&
           mqttClient_.publish(topic, payload, false, 1);
}

bool BoilerService::buildTopics() {
    return buildTopic(topics_.climate, sizeof(topics_.climate),
                      CLIMATE_SUFFIX) &&
           buildTopic(topics_.boilerPower, sizeof(topics_.boilerPower),
                      BOILER_POWER_SUFFIX) &&
           buildTopic(topics_.snapshotRequest,
                      sizeof(topics_.snapshotRequest), SNAPSHOT_SUFFIX) &&
           buildTopic(topics_.commandHvacMode,
                      sizeof(topics_.commandHvacMode), HVAC_COMMAND_SUFFIX) &&
           buildTopic(topics_.commandPresetMode,
                      sizeof(topics_.commandPresetMode),
                      PRESET_COMMAND_SUFFIX) &&
           buildTopic(topics_.commandTemperature,
                      sizeof(topics_.commandTemperature),
                      TEMPERATURE_COMMAND_SUFFIX) &&
           buildTopic(topics_.commandBoilerPower,
                      sizeof(topics_.commandBoilerPower),
                      POWER_COMMAND_SUFFIX);
}

bool BoilerService::buildTopic(char* target, size_t size,
                               const char* suffix) {
    const int length = snprintf(target, size, "%s%s",
                                AppConfig::MQTT_BASE_TOPIC, suffix);
    return length > 0 && static_cast<size_t>(length) < size;
}

void BoilerService::scheduleRetry() {
    static const uint8_t multipliers[] = {1, 2, 5, 10, 30};
    const uint8_t index = min<uint8_t>(backoffStep_, 4);
    retryDelayMs_ = min(
        AppConfig::MQTT_RECONNECT_MIN_MS * multipliers[index],
        AppConfig::MQTT_RECONNECT_MAX_MS);
    if (backoffStep_ < 4) ++backoffStep_;
    phase_ = Phase::WAIT_RETRY;
    retryStartedMs_ = millis();
    Serial.printf("MQTT RETRY: %lu ms\n",
                  static_cast<unsigned long>(retryDelayMs_));
}
