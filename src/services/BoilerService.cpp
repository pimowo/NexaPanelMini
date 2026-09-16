#include "services/BoilerService.h"
#include <ArduinoJson.h>
#include <ctype.h>
#include <math.h>
#include "config.h"
#include "version.h"

namespace {
constexpr char CLIMATE_SUFFIX[] = "/ha/state/climate";
constexpr char BOILER_POWER_SUFFIX[] = "/ha/state/boiler_power";
constexpr char SNAPSHOT_SUFFIX[] = "/ha/snapshot/request";
constexpr char HVAC_COMMAND_SUFFIX[] = "/ha/command/climate/hvac_mode";
constexpr char PRESET_COMMAND_SUFFIX[] = "/ha/command/climate/preset_mode";
constexpr char TEMPERATURE_COMMAND_SUFFIX[] =
    "/ha/command/climate/temperature";
constexpr char POWER_COMMAND_SUFFIX[] = "/ha/command/boiler_power";
constexpr char PANEL_STATUS_SUFFIX[] = "/status";
constexpr char PANEL_RSSI_SUFFIX[] = "/rssi/state";
constexpr char PANEL_UPTIME_SUFFIX[] = "/uptime/state";
constexpr char PANEL_FIRMWARE_SUFFIX[] = "/firmware/state";
constexpr char PANEL_RESTART_SET_SUFFIX[] = "/restart/set";
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

bool parseFloatPayload(const String& payload, float& outValue) {
    char* end = nullptr;
    const float parsed = strtof(payload.c_str(), &end);
    if (end == payload.c_str()) return false;
    while (*end != '\0') {
        if (!isspace(static_cast<unsigned char>(*end))) return false;
        ++end;
    }
    if (!isfinite(parsed)) return false;
    outValue = parsed;
    return true;
}
}

void BoilerService::begin(AppState& state) {
    state_ = &state;
    retryDelayMs_ = AppConfig::MQTT_RECONNECT_MIN_MS;
    if (AppConfig::MQTT_HOST[0] == '\0' ||
        AppConfig::MQTT_CLIENT_ID[0] == '\0' ||
        AppConfig::BOILER_MQTT_BASE_TOPIC[0] == '\0' ||
        AppConfig::PANEL_MQTT_BASE_TOPIC[0] == '\0' || !buildTopics()) {
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

    const uint32_t now = millis();
    if (state_->haOutsideTempValid &&
        static_cast<uint32_t>(now - state_->haOutsideTempLastUpdateMs) >=
            AppConfig::HA_OUTSIDE_TEMP_STALE_MS) {
        state_->haOutsideTempValid = false;
        if (!outsideTempStaleLogged_) {
            Serial.println("HA OUTSIDE TEMP: stale, using Open-Meteo");
            outsideTempStaleLogged_ = true;
        }
    }
    if (state_->haOutsidePressureValid &&
        static_cast<uint32_t>(now - state_->haOutsidePressureLastUpdateMs) >=
            AppConfig::HA_OUTSIDE_TEMP_STALE_MS) {
        state_->haOutsidePressureValid = false;
        if (!outsidePressureStaleLogged_) {
            Serial.println("HA OUTSIDE PRESSURE: stale, using Open-Meteo");
            outsidePressureStaleLogged_ = true;
        }
    }

    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (!wifiConnected) {
        if (wifiWasConnected_ || phase_ != Phase::IDLE) {
            handleDisconnected(false);
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
        return;
    }

    if (restartRequested_) {
        restartRequested_ = false;
        mqttClient_.publish(topics_.panelAvailability, "offline", true, 1);
        mqttClient_.disconnect();
        delay(50);
        ESP.restart();
        return;
    }

    if (phase_ == Phase::CONNECTED) {
        if (static_cast<int32_t>(now - nextPanelTelemetryMs_) >= 0) {
            publishPanelTelemetry(false);
            nextPanelTelemetryMs_ = now + AppConfig::PANEL_MQTT_DIAGNOSTIC_PUBLISH_MS;
        }
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
    mqttClient_.setWill(topics_.panelAvailability, "offline", true, 1);
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
        mqttClient_.subscribe(topics_.boilerPower, 1) &&
        mqttClient_.subscribe(topics_.panelOutsideTemperatureState, 1) &&
        mqttClient_.subscribe(topics_.panelOutsidePressureState, 1) &&
        mqttClient_.subscribe(topics_.panelRestartSet, 1);
    if (!subscribed ||
        !publishPanelAvailabilityOnline() ||
        !publishPanelDiscovery() ||
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
    publishPanelTelemetry(true);
    nextPanelTelemetryMs_ = millis() + AppConfig::PANEL_MQTT_DIAGNOSTIC_PUBLISH_MS;
    Serial.println("MQTT CONNECTED");
    Serial.println("MQTT DISCOVERY published");
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
    if (state_ && state_->haOutsideTempValid) {
        state_->haOutsideTempValid = false;
        outsideTempStaleLogged_ = false;
        Serial.println("HA OUTSIDE TEMP: unavailable");
    }
    if (state_ && state_->haOutsidePressureValid) {
        state_->haOutsidePressureValid = false;
        outsidePressureStaleLogged_ = false;
        Serial.println("HA OUTSIDE PRESSURE: unavailable");
    }
    if (shouldLog) Serial.println("MQTT DISCONNECTED");
    if (retry && WiFi.status() == WL_CONNECTED) scheduleRetry();
}

void BoilerService::handleMessage(String& topic, String& payload) {
    if (topic == topics_.climate) parseClimate(payload);
    else if (topic == topics_.boilerPower) parseBoilerPower(payload);
    else if (topic == topics_.panelOutsideTemperatureState) {
        parseOutsideTemperature(payload);
    }
    else if (topic == topics_.panelOutsidePressureState) {
        parseOutsidePressure(payload);
    }
    else if (topic == topics_.panelRestartSet) parsePanelRestartCommand(payload);
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
        BoilerPreset parsedPreset = BoilerPreset::MANUAL;
        if (preset.equalsIgnoreCase("comfort")) {
            parsedPreset = BoilerPreset::COMFORT;
        } else if (preset.equalsIgnoreCase("sleep")) {
            parsedPreset = BoilerPreset::SLEEP;
        } else if (preset.equalsIgnoreCase("manual")) {
            parsedPreset = BoilerPreset::MANUAL;
        }
        if (state_->boilerPreset != parsedPreset) {
            state_->boilerPreset = parsedPreset;
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

void BoilerService::parsePanelRestartCommand(String payload) {
    payload.trim();
    if (payload.equalsIgnoreCase("PRESS")) {
        Serial.println("MQTT PANEL RESTART requested");
        restartRequested_ = true;
    }
}

void BoilerService::parseOutsideTemperature(String payload) {
    payload.trim();

    const bool unavailable = payload.length() == 0 ||
                             payload.equalsIgnoreCase("unavailable") ||
                             payload.equalsIgnoreCase("unknown");
    float value = NAN;
    if (unavailable || !parseFloatPayload(payload, value)) {
        if (state_->haOutsideTempValid) {
            state_->haOutsideTempValid = false;
            outsideTempStaleLogged_ = false;
            Serial.println("HA OUTSIDE TEMP: unavailable");
        }
        return;
    }

    if (!state_->haOutsideTempValid && outsideTempHadValidSample_) {
        Serial.println("HA OUTSIDE TEMP: restored");
    }
    if (!state_->haOutsideTempValid ||
        differentFloat(state_->haOutsideTemp, value)) {
        Serial.printf("HA OUTSIDE TEMP: %.1f C\n", value);
    }
    state_->haOutsideTemp = value;
    state_->haOutsideTempValid = true;
    state_->haOutsideTempLastUpdateMs = millis();
    outsideTempHadValidSample_ = true;
    outsideTempStaleLogged_ = false;
}

void BoilerService::parseOutsidePressure(String payload) {
    payload.trim();

    const bool unavailable = payload.length() == 0 ||
                             payload.equalsIgnoreCase("unavailable") ||
                             payload.equalsIgnoreCase("unknown");
    float value = NAN;
    if (unavailable || !parseFloatPayload(payload, value)) {
        if (state_->haOutsidePressureValid) {
            state_->haOutsidePressureValid = false;
            outsidePressureStaleLogged_ = false;
            Serial.println("HA OUTSIDE PRESSURE: unavailable");
        }
        return;
    }

    if (!state_->haOutsidePressureValid && outsidePressureHadValidSample_) {
        Serial.println("HA OUTSIDE PRESSURE: restored");
    }
    if (!state_->haOutsidePressureValid ||
        differentFloat(state_->haOutsidePressure, value)) {
        Serial.printf("HA OUTSIDE PRESSURE: %.0f hPa\n", value);
    }
    state_->haOutsidePressure = value;
    state_->haOutsidePressureValid = true;
    state_->haOutsidePressureLastUpdateMs = millis();
    outsidePressureHadValidSample_ = true;
    outsidePressureStaleLogged_ = false;
}

bool BoilerService::publishCommand(const char* topic, const char* payload) {
    return phase_ == Phase::CONNECTED && mqttClient_.connected() &&
           mqttClient_.publish(topic, payload, false, 1);
}

bool BoilerService::buildTopics() {
    return buildBoilerTopic(topics_.climate, sizeof(topics_.climate),
                            CLIMATE_SUFFIX) &&
           buildBoilerTopic(topics_.boilerPower, sizeof(topics_.boilerPower),
                            BOILER_POWER_SUFFIX) &&
           buildBoilerTopic(topics_.snapshotRequest,
                            sizeof(topics_.snapshotRequest), SNAPSHOT_SUFFIX) &&
           buildBoilerTopic(topics_.commandHvacMode,
                            sizeof(topics_.commandHvacMode), HVAC_COMMAND_SUFFIX) &&
           buildBoilerTopic(topics_.commandPresetMode,
                            sizeof(topics_.commandPresetMode),
                            PRESET_COMMAND_SUFFIX) &&
           buildBoilerTopic(topics_.commandTemperature,
                            sizeof(topics_.commandTemperature),
                            TEMPERATURE_COMMAND_SUFFIX) &&
           buildBoilerTopic(topics_.commandBoilerPower,
                            sizeof(topics_.commandBoilerPower),
                            POWER_COMMAND_SUFFIX) &&
           buildPanelTopic(topics_.panelAvailability,
                           sizeof(topics_.panelAvailability),
                           PANEL_STATUS_SUFFIX) &&
           buildPanelTopic(topics_.panelRssiState,
                           sizeof(topics_.panelRssiState),
                           PANEL_RSSI_SUFFIX) &&
           buildPanelTopic(topics_.panelUptimeState,
                           sizeof(topics_.panelUptimeState),
                           PANEL_UPTIME_SUFFIX) &&
           buildPanelTopic(topics_.panelFirmwareState,
                           sizeof(topics_.panelFirmwareState),
                           PANEL_FIRMWARE_SUFFIX) &&
           snprintf(topics_.panelOutsideTemperatureState,
                    sizeof(topics_.panelOutsideTemperatureState), "%s",
                    AppConfig::HA_SHARED_OUTSIDE_TEMPERATURE_TOPIC) > 0 &&
           snprintf(topics_.panelOutsidePressureState,
                    sizeof(topics_.panelOutsidePressureState), "%s",
                    AppConfig::HA_SHARED_OUTSIDE_PRESSURE_TOPIC) > 0 &&
           buildPanelTopic(topics_.panelRestartSet,
                           sizeof(topics_.panelRestartSet),
                           PANEL_RESTART_SET_SUFFIX) &&
           buildDiscoveryTopic(topics_.discoveryRssi,
                               sizeof(topics_.discoveryRssi),
                               "sensor", "rssi") &&
           buildDiscoveryTopic(topics_.discoveryUptime,
                               sizeof(topics_.discoveryUptime),
                               "sensor", "uptime") &&
           buildDiscoveryTopic(topics_.discoveryFirmware,
                               sizeof(topics_.discoveryFirmware),
                               "sensor", "firmware") &&
           buildDiscoveryTopic(topics_.discoveryRestart,
                               sizeof(topics_.discoveryRestart),
                               "button", "restart");
}

bool BoilerService::buildBoilerTopic(char* target, size_t size,
                                     const char* suffix) {
    const int length = snprintf(target, size, "%s%s",
                                AppConfig::BOILER_MQTT_BASE_TOPIC, suffix);
    return length > 0 && static_cast<size_t>(length) < size;
}

bool BoilerService::buildPanelTopic(char* target, size_t size,
                                    const char* suffix) {
    const int length = snprintf(target, size, "%s%s",
                                AppConfig::PANEL_MQTT_BASE_TOPIC, suffix);
    return length > 0 && static_cast<size_t>(length) < size;
}

bool BoilerService::buildDiscoveryTopic(char* target, size_t size,
                                        const char* component,
                                        const char* objectId) {
    const int length = snprintf(target, size, "%s/%s/%s/%s/config",
                                AppConfig::PANEL_MQTT_DISCOVERY_PREFIX,
                                component,
                                AppConfig::PANEL_MQTT_BASE_TOPIC,
                                objectId);
    return length > 0 && static_cast<size_t>(length) < size;
}

bool BoilerService::publishPanelAvailabilityOnline() {
    return mqttClient_.publish(topics_.panelAvailability, "online", true, 1);
}

bool BoilerService::publishPanelDiscovery() {
    StaticJsonDocument<512> document;
    JsonObject device = document.createNestedObject("dev");
    JsonArray ids = device.createNestedArray("ids");
    ids.add(AppConfig::PANEL_DEVICE_IDENTIFIER);
    device["name"] = AppConfig::PANEL_DEVICE_NAME;
    device["mf"] = AppConfig::PANEL_DEVICE_MANUFACTURER;
    device["mdl"] = AppConfig::PANEL_DEVICE_MODEL;
    device["sw"] = FW_VERSION;

    document["name"] = "Wi-Fi RSSI";
    document["uniq_id"] = "nexapanel-mini_rssi";
    document["stat_t"] = topics_.panelRssiState;
    document["avty_t"] = topics_.panelAvailability;
    document["pl_avail"] = "online";
    document["pl_not_avail"] = "offline";
    document["unit_of_meas"] = "dBm";
    document["dev_cla"] = "signal_strength";
    document["ent_cat"] = "diagnostic";
    char payload[512]{};
    size_t length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || !mqttClient_.publish(topics_.discoveryRssi, payload,
                                            true, 1)) {
        return false;
    }

    document.clear();
    device = document.createNestedObject("dev");
    ids = device.createNestedArray("ids");
    ids.add(AppConfig::PANEL_DEVICE_IDENTIFIER);
    device["name"] = AppConfig::PANEL_DEVICE_NAME;
    device["mf"] = AppConfig::PANEL_DEVICE_MANUFACTURER;
    device["mdl"] = AppConfig::PANEL_DEVICE_MODEL;
    device["sw"] = FW_VERSION;
    document["name"] = "Uptime";
    document["uniq_id"] = "nexapanel-mini_uptime";
    document["stat_t"] = topics_.panelUptimeState;
    document["avty_t"] = topics_.panelAvailability;
    document["pl_avail"] = "online";
    document["pl_not_avail"] = "offline";
    document["unit_of_meas"] = "s";
    document["ent_cat"] = "diagnostic";
    length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || !mqttClient_.publish(topics_.discoveryUptime, payload,
                                            true, 1)) {
        return false;
    }

    document.clear();
    device = document.createNestedObject("dev");
    ids = device.createNestedArray("ids");
    ids.add(AppConfig::PANEL_DEVICE_IDENTIFIER);
    device["name"] = AppConfig::PANEL_DEVICE_NAME;
    device["mf"] = AppConfig::PANEL_DEVICE_MANUFACTURER;
    device["mdl"] = AppConfig::PANEL_DEVICE_MODEL;
    device["sw"] = FW_VERSION;
    document["name"] = "Firmware";
    document["uniq_id"] = "nexapanel-mini_firmware";
    document["stat_t"] = topics_.panelFirmwareState;
    document["avty_t"] = topics_.panelAvailability;
    document["pl_avail"] = "online";
    document["pl_not_avail"] = "offline";
    document["ent_cat"] = "diagnostic";
    document["icon"] = "mdi:chip";
    length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || !mqttClient_.publish(topics_.discoveryFirmware, payload,
                                            true, 1)) {
        return false;
    }

    document.clear();
    device = document.createNestedObject("dev");
    ids = device.createNestedArray("ids");
    ids.add(AppConfig::PANEL_DEVICE_IDENTIFIER);
    device["name"] = AppConfig::PANEL_DEVICE_NAME;
    device["mf"] = AppConfig::PANEL_DEVICE_MANUFACTURER;
    device["mdl"] = AppConfig::PANEL_DEVICE_MODEL;
    device["sw"] = FW_VERSION;
    document["name"] = "Restart";
    document["uniq_id"] = "nexapanel-mini_restart";
    document["cmd_t"] = topics_.panelRestartSet;
    document["avty_t"] = topics_.panelAvailability;
    document["pl_avail"] = "online";
    document["pl_not_avail"] = "offline";
    document["pl_prs"] = "PRESS";
    document["ent_cat"] = "diagnostic";
    length = serializeJson(document, payload, sizeof(payload));
    return length > 0 && mqttClient_.publish(topics_.discoveryRestart, payload,
                                             true, 1);
}

void BoilerService::publishPanelTelemetry(bool force) {
    if (!state_) return;

    const int rssi = state_->wifiRssi;
    if (force || rssi != lastPublishedRssi_) {
        char payload[16]{};
        snprintf(payload, sizeof(payload), "%d", rssi);
        if (mqttClient_.publish(topics_.panelRssiState, payload, false, 0)) {
            lastPublishedRssi_ = rssi;
        }
    }

    const uint32_t uptimeSeconds = millis() / 1000UL;
    if (force || uptimeSeconds != lastPublishedUptimeS_) {
        char payload[16]{};
        snprintf(payload, sizeof(payload), "%lu",
                 static_cast<unsigned long>(uptimeSeconds));
        if (mqttClient_.publish(topics_.panelUptimeState, payload, false, 0)) {
            lastPublishedUptimeS_ = uptimeSeconds;
        }
    }

    if (force) {
        mqttClient_.publish(topics_.panelFirmwareState, FW_VERSION, true, 1);
    }
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
