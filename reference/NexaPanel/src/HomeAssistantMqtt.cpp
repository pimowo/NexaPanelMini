#include "HomeAssistantMqtt.h"

#include <ArduinoJson.h>
#include <esp_mac.h>

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "AppState.h"
#include "FirmwareInfo.h"
#include "RuntimeConfig.h"

namespace {

constexpr char STATUS_SUFFIX[] = "/status";
constexpr char SNAPSHOT_REQUEST_SUFFIX[] = "/ha/snapshot/request";
constexpr char DIAGNOSTIC_YORADIO_SUFFIX[] = "/diagnostic/yoradio";
constexpr char DIAGNOSTIC_NEXTION_SUFFIX[] = "/diagnostic/nextion";
constexpr char DIAGNOSTIC_WIFI_RSSI_SUFFIX[] = "/diagnostic/wifi_rssi";
constexpr char DIAGNOSTIC_UPTIME_SUFFIX[] = "/diagnostic/uptime";
constexpr char DIAGNOSTIC_FIRMWARE_SUFFIX[] = "/diagnostic/firmware";
constexpr uint8_t DISCOVERY_ENTITY_COUNT = 6;
constexpr uint8_t DIAGNOSTIC_ENTITY_COUNT = 5;
constexpr uint8_t PERIODIC_DIAGNOSTIC_COUNT = 2;
constexpr uint8_t HOME_ASSISTANT_GROUP_COUNT = 6;

struct TextSpan {
  const char* data{nullptr};
  size_t length{0};
};

TextSpan trimmedSpan(const String& text) {
  TextSpan span{text.c_str(), text.length()};
  while (span.length > 0 &&
         isspace(static_cast<unsigned char>(*span.data))) {
    ++span.data;
    --span.length;
  }
  while (span.length > 0 &&
         isspace(static_cast<unsigned char>(span.data[span.length - 1]))) {
    --span.length;
  }
  return span;
}

TextSpan trimmedSpan(const char* text) {
  TextSpan span{text != nullptr ? text : "", text != nullptr ? strlen(text) : 0};
  while (span.length > 0 &&
         isspace(static_cast<unsigned char>(*span.data))) {
    ++span.data;
    --span.length;
  }
  while (span.length > 0 &&
         isspace(static_cast<unsigned char>(span.data[span.length - 1]))) {
    --span.length;
  }
  return span;
}

bool spanEqualsIgnoreCase(const TextSpan& span, const char* expected) {
  const size_t expectedLength = strlen(expected);
  if (span.length != expectedLength) {
    return false;
  }
  for (size_t index = 0; index < span.length; ++index) {
    if (tolower(static_cast<unsigned char>(span.data[index])) !=
        tolower(static_cast<unsigned char>(expected[index]))) {
      return false;
    }
  }
  return true;
}

bool isUnavailableText(const TextSpan& span) {
  return span.length == 0 || spanEqualsIgnoreCase(span, "unavailable") ||
         spanEqualsIgnoreCase(span, "unknown") ||
         spanEqualsIgnoreCase(span, "null");
}

template <size_t N>
void setInvalidText(StateText<N>& target) {
  target.value[0] = '\0';
  target.received = true;
  target.valid = false;
}

template <size_t N>
void setTextValue(StateText<N>& target, const TextSpan& span) {
  if (isUnavailableText(span)) {
    setInvalidText(target);
    return;
  }

  size_t copyLength = span.length < N ? span.length : N;
  if (copyLength < span.length) {
    // Nie pozostawiamy na końcu niepełnej sekwencji UTF-8.
    while (copyLength > 0 &&
           (static_cast<uint8_t>(span.data[copyLength]) & 0xC0U) == 0x80U) {
      --copyLength;
    }
  }
  memcpy(target.value, span.data, copyLength);
  target.value[copyLength] = '\0';
  target.received = true;
  target.valid = copyLength > 0;
}

template <size_t N>
void setJsonText(StateText<N>& target, JsonVariantConst value) {
  if (value.isNull() || !value.is<const char*>()) {
    setInvalidText(target);
    return;
  }
  const char* text = value.as<const char*>();
  setTextValue(target, trimmedSpan(text));
}

bool readJsonNumber(JsonVariantConst value, float& result) {
  if (!(value.is<float>() || value.is<double>() || value.is<int>() ||
        value.is<long>() || value.is<unsigned int>() ||
        value.is<unsigned long>())) {
    return false;
  }
  result = value.as<float>();
  return isfinite(result);
}

}  // namespace

HomeAssistantMqtt::HomeAssistantMqtt(AppState& state,
                                     const RuntimeConfig& config)
    : state_(state), config_(config),
      mqttClient_(Config::Mqtt::BUFFER_SIZE) {}

void HomeAssistantMqtt::begin() {
  state_.homeAssistant.mqttConnected = false;
  state_.homeAssistant.mqttConnecting = false;
  state_.homeAssistant.homeAssistantOnline = false;

  if (config_.mqttHost()[0] == '\0' ||
      config_.mqttClientId()[0] == '\0' ||
      config_.mqttBaseTopic()[0] == '\0') {
    Serial.println("MQTT CONFIG ERROR: brak hosta, client ID lub base topic");
    return;
  }

  configurationAvailable_ = initializeIdentity() && buildTopics();
  if (!configurationAvailable_) {
    Serial.println("MQTT CONFIG ERROR: topic lub identyfikator jest za długi");
    return;
  }

  networkClient_.setConnectionTimeout(Config::Mqtt::CONNECT_TIMEOUT_MS);
  mqttClient_.begin(config_.mqttHost(), config_.mqttPort(),
                    networkClient_);
  mqttClient_.setOptions(Config::Mqtt::KEEPALIVE_SECONDS, true,
                         Config::Mqtt::OPERATION_TIMEOUT_MS);
  mqttClient_.setWill(topics_.status, "offline", true, 1);
  mqttClient_.onMessage(
      [this](String& topic, String& payload) {
        handleMessage(topic, payload);
      });
}

void HomeAssistantMqtt::loop() {
  if (!configurationAvailable_) {
    return;
  }

  const uint32_t now = millis();
  if (!state_.network.connected) {
    if (wifiWasConnected_ || state_.homeAssistant.mqttConnected ||
        state_.homeAssistant.mqttConnecting ||
        phase_ != Phase::IDLE) {
      handleWifiLost();
    }
    return;
  }

  if (!wifiWasConnected_ ||
      observedWifiGeneration_ != state_.network.sessionGeneration) {
    networkClient_.stop();
    wifiWasConnected_ = true;
    observedWifiGeneration_ = state_.network.sessionGeneration;
    phase_ = Phase::IDLE;
    backoffStep_ = 0;
    attemptConnection();
    return;
  }

  if (phase_ == Phase::WAIT_RETRY) {
    if (static_cast<uint32_t>(now - retryStartedMs_) >= retryDelayMs_) {
      attemptConnection();
    }
    return;
  }

  if (phase_ != Phase::CONNECTED) {
    attemptConnection();
    return;
  }

  if (!mqttClient_.connected() || !mqttClient_.loop()) {
    handleDisconnected(true);
    return;
  }

  serviceSnapshotSynchronization(now);

  if (!periodicDiagnosticsActive_ &&
      static_cast<uint32_t>(now - lastPeriodicDiagnosticsMs_) >=
          Config::Mqtt::DIAGNOSTIC_INTERVAL_MS) {
    periodicDiagnosticsActive_ = true;
    periodicDiagnosticIndex_ = 0;
    lastPeriodicDiagnosticsMs_ = now;
  }

  servicePendingPublications();
}

void HomeAssistantMqtt::requestSnapshot() {
  snapshotRequestPending_ = true;
}

bool HomeAssistantMqtt::publishHvacMode(HvacMode mode) {
  if (mode == HvacMode::HEAT) {
    return publishCommand(topics_.commandHvacMode, "heat");
  }
  if (mode == HvacMode::OFF) {
    return publishCommand(topics_.commandHvacMode, "off");
  }
  return false;
}

bool HomeAssistantMqtt::publishPresetMode(PresetMode mode) {
  if (mode == PresetMode::COMFORT) {
    return publishCommand(topics_.commandPresetMode, "comfort");
  }
  if (mode == PresetMode::SLEEP) {
    return publishCommand(topics_.commandPresetMode, "sleep");
  }
  return false;
}

bool HomeAssistantMqtt::publishTargetTemperature(float temperature) {
  if (!isfinite(temperature) ||
      temperature < Config::Boiler::TARGET_MIN_C ||
      temperature > Config::Boiler::TARGET_MAX_C) {
    return false;
  }

  char payload[16]{};
  const int length =
      snprintf(payload, sizeof(payload), "%.1f", temperature);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload)) {
    return false;
  }
  return publishCommand(topics_.commandTemperature, payload);
}

bool HomeAssistantMqtt::publishBoilerPower(BinaryState state) {
  if (state == BinaryState::ON) {
    return publishCommand(topics_.commandBoilerPower, "ON");
  }
  if (state == BinaryState::OFF) {
    return publishCommand(topics_.commandBoilerPower, "OFF");
  }
  return false;
}

bool HomeAssistantMqtt::initializeIdentity() {
  uint8_t mac[6]{};
  const bool macAvailable =
      esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK;

  int identifierLength = 0;
  if (macAvailable) {
    identifierLength = snprintf(
        deviceIdentifier_, sizeof(deviceIdentifier_),
        "%s_%02x%02x%02x%02x%02x%02x", Config::Identity::DEVICE_ID,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    identifierLength =
        snprintf(deviceIdentifier_, sizeof(deviceIdentifier_), "%s",
                 Config::Identity::DEVICE_ID);
  }

  const int objectLength =
      snprintf(objectId_, sizeof(objectId_), "%s", deviceIdentifier_);
  return identifierLength > 0 &&
         static_cast<size_t>(identifierLength) < sizeof(deviceIdentifier_) &&
         objectLength > 0 &&
         static_cast<size_t>(objectLength) < sizeof(objectId_);
}

bool HomeAssistantMqtt::buildTopics() {
  const int haStatusLength =
      snprintf(topics_.homeAssistantStatus,
               sizeof(topics_.homeAssistantStatus), "%s/status",
               config_.haDiscoveryPrefix());

  return haStatusLength > 0 &&
         static_cast<size_t>(haStatusLength) <
             sizeof(topics_.homeAssistantStatus) &&
         buildTopic(topics_.status, sizeof(topics_.status), STATUS_SUFFIX) &&
         buildTopic(topics_.snapshotRequest, sizeof(topics_.snapshotRequest),
                    SNAPSHOT_REQUEST_SUFFIX) &&
         buildTopic(topics_.weather, sizeof(topics_.weather),
                    HomeAssistantTopicSuffix::STATE_WEATHER) &&
         buildTopic(topics_.climate, sizeof(topics_.climate),
                    HomeAssistantTopicSuffix::STATE_CLIMATE) &&
         buildTopic(topics_.boilerPower, sizeof(topics_.boilerPower),
                    HomeAssistantTopicSuffix::STATE_BOILER_POWER) &&
         buildTopic(topics_.windows, sizeof(topics_.windows),
                    HomeAssistantTopicSuffix::STATE_WINDOWS) &&
         buildTopic(topics_.calendar, sizeof(topics_.calendar),
                    HomeAssistantTopicSuffix::STATE_CALENDAR) &&
         buildTopic(topics_.waste, sizeof(topics_.waste),
                    HomeAssistantTopicSuffix::STATE_WASTE) &&
         buildTopic(topics_.commandHvacMode,
                    sizeof(topics_.commandHvacMode),
                    HomeAssistantTopicSuffix::COMMAND_HVAC_MODE) &&
         buildTopic(topics_.commandPresetMode,
                    sizeof(topics_.commandPresetMode),
                    HomeAssistantTopicSuffix::COMMAND_PRESET_MODE) &&
         buildTopic(topics_.commandTemperature,
                    sizeof(topics_.commandTemperature),
                    HomeAssistantTopicSuffix::COMMAND_TEMPERATURE) &&
         buildTopic(topics_.commandBoilerPower,
                    sizeof(topics_.commandBoilerPower),
                    HomeAssistantTopicSuffix::COMMAND_BOILER_POWER) &&
         buildTopic(topics_.yoRadio, sizeof(topics_.yoRadio),
                    DIAGNOSTIC_YORADIO_SUFFIX) &&
         buildTopic(topics_.nextion, sizeof(topics_.nextion),
                    DIAGNOSTIC_NEXTION_SUFFIX) &&
         buildTopic(topics_.wifiRssi, sizeof(topics_.wifiRssi),
                    DIAGNOSTIC_WIFI_RSSI_SUFFIX) &&
         buildTopic(topics_.uptime, sizeof(topics_.uptime),
                    DIAGNOSTIC_UPTIME_SUFFIX) &&
         buildTopic(topics_.firmware, sizeof(topics_.firmware),
                    DIAGNOSTIC_FIRMWARE_SUFFIX);
}

bool HomeAssistantMqtt::buildTopic(char* target, size_t targetSize,
                                   const char* suffix) {
  const int result =
      snprintf(target, targetSize, "%s%s", config_.mqttBaseTopic(), suffix);
  return result > 0 && static_cast<size_t>(result) < targetSize;
}

void HomeAssistantMqtt::attemptConnection() {
  if (!state_.network.connected) {
    return;
  }

  networkClient_.stop();
  ++state_.diagnostics.mqtt.connectionAttempts;
  state_.homeAssistant.mqttConnecting = true;
  state_.homeAssistant.mqttConnected = false;
  Serial.println("MQTT CONNECTING");

  const bool connected =
      config_.mqttUsername()[0] == '\0'
          ? mqttClient_.connect(config_.mqttClientId())
          : mqttClient_.connect(config_.mqttClientId(),
                                config_.mqttUsername(),
                                 config_.mqttPassword());
  state_.diagnostics.mqtt.lastError =
      static_cast<int32_t>(mqttClient_.lastError());
  state_.diagnostics.mqtt.lastReturnCode =
      static_cast<int32_t>(mqttClient_.returnCode());
  if (!connected) {
    handleDisconnected(true);
    return;
  }

  handleConnected();
}

void HomeAssistantMqtt::handleConnected() {
  Serial.println("MQTT CONNECTED");

  if (!publishMessage(topics_.status, "online", true, 1)) {
    handleDisconnected(true);
    return;
  }
  Serial.println("MQTT BIRTH online");

  if (!mqttClient_.subscribe(topics_.homeAssistantStatus, 1)) {
    handleDisconnected(true);
    return;
  }
  if (!subscribeStateTopics()) {
    handleDisconnected(true);
    return;
  }

  const bool reconnect = state_.diagnostics.mqtt.successfulConnections > 0;
  phase_ = Phase::CONNECTED;
  state_.homeAssistant.mqttConnecting = false;
  state_.homeAssistant.mqttConnected = true;
  state_.homeAssistant.homeAssistantOnline = false;
  ++state_.homeAssistant.mqttSessionGeneration;
  if (state_.homeAssistant.mqttSessionGeneration == 0U) {
    ++state_.homeAssistant.mqttSessionGeneration;
  }
  state_.homeAssistant.mqttLastConnectedMs = millis();
  ++state_.diagnostics.mqtt.successfulConnections;
  if (reconnect) {
    ++state_.diagnostics.mqtt.reconnects;
  }
  state_.diagnostics.mqtt.lastSuccessfulConnectionMs =
      state_.homeAssistant.mqttLastConnectedMs;
  backoffStep_ = 0;

  invalidatePublishCache();
  startSnapshotSynchronization();
  startDiscoveryPublish();
  startDiagnosticSnapshot();
  lastPeriodicDiagnosticsMs_ = millis();
}

void HomeAssistantMqtt::handleDisconnected(bool scheduleReconnect) {
  const bool wasConnected = state_.homeAssistant.mqttConnected ||
                            phase_ == Phase::CONNECTED;
  const bool shouldLog = state_.homeAssistant.mqttConnected ||
                         state_.homeAssistant.mqttConnecting ||
                         phase_ == Phase::CONNECTED;
  networkClient_.stop();
  phase_ = Phase::IDLE;
  state_.homeAssistant.mqttConnected = false;
  state_.homeAssistant.mqttConnecting = false;
  state_.homeAssistant.homeAssistantOnline = false;
  state_.homeAssistant.mqttLastDisconnectedMs = millis();
  state_.diagnostics.mqtt.lastError =
      static_cast<int32_t>(mqttClient_.lastError());
  state_.diagnostics.mqtt.lastReturnCode =
      static_cast<int32_t>(mqttClient_.returnCode());
  if (wasConnected) {
    ++state_.diagnostics.mqtt.disconnects;
    state_.diagnostics.mqtt.lastDisconnectedMs =
        state_.homeAssistant.mqttLastDisconnectedMs;
  }
  snapshotRequestPending_ = false;
  snapshotSynchronizationActive_ = false;
  discoveryPublishActive_ = false;
  diagnosticSnapshotActive_ = false;
  periodicDiagnosticsActive_ = false;
  invalidatePublishCache();

  if (shouldLog) {
    Serial.println("MQTT DISCONNECTED");
  }
  if (scheduleReconnect && state_.network.connected) {
    scheduleRetry();
  }
}

void HomeAssistantMqtt::handleWifiLost() {
  const bool hadSession = state_.homeAssistant.mqttConnected ||
                          state_.homeAssistant.mqttConnecting ||
                          phase_ == Phase::CONNECTED;
  // Nagłe zamknięcie TCP pozwala brokerowi opublikować skonfigurowany LWT.
  networkClient_.stop();
  wifiWasConnected_ = false;
  phase_ = Phase::IDLE;
  state_.homeAssistant.mqttConnected = false;
  state_.homeAssistant.mqttConnecting = false;
  state_.homeAssistant.homeAssistantOnline = false;
  state_.homeAssistant.mqttLastDisconnectedMs = millis();
  snapshotRequestPending_ = false;
  snapshotSynchronizationActive_ = false;
  discoveryPublishActive_ = false;
  diagnosticSnapshotActive_ = false;
  periodicDiagnosticsActive_ = false;
  invalidatePublishCache();

  if (hadSession) {
    Serial.println("MQTT DISCONNECTED");
  }
  if (state_.homeAssistant.mqttSessionGeneration > 0 && hadSession) {
    ++state_.diagnostics.mqtt.disconnects;
    state_.diagnostics.mqtt.lastDisconnectedMs =
        state_.homeAssistant.mqttLastDisconnectedMs;
  }
}

void HomeAssistantMqtt::scheduleRetry() {
  phase_ = Phase::WAIT_RETRY;
  retryStartedMs_ = millis();

  uint32_t multiplier = 1U;
  if (backoffStep_ == 1U) {
    multiplier = 2U;
  } else if (backoffStep_ == 2U) {
    multiplier = 5U;
  } else if (backoffStep_ == 3U) {
    multiplier = 10U;
  } else if (backoffStep_ >= 4U) {
    retryDelayMs_ = Config::Mqtt::RECONNECT_MAX_MS;
    Serial.printf("MQTT RETRY in %lu ms\n",
                  static_cast<unsigned long>(retryDelayMs_));
    return;
  }

  const uint64_t candidate =
      static_cast<uint64_t>(Config::Mqtt::RECONNECT_MIN_MS) * multiplier;
  retryDelayMs_ = candidate > Config::Mqtt::RECONNECT_MAX_MS
                      ? Config::Mqtt::RECONNECT_MAX_MS
                      : static_cast<uint32_t>(candidate);
  Serial.printf("MQTT RETRY in %lu ms\n",
                static_cast<unsigned long>(retryDelayMs_));
  advanceBackoff();
}

void HomeAssistantMqtt::advanceBackoff() {
  if (backoffStep_ < 4U) {
    ++backoffStep_;
  }
}

void HomeAssistantMqtt::handleMessage(String& topic, String& payload) {
  if (Config::Logging::DEBUG_MQTT) {
    Serial.printf("MQTT RX topic=%s payload=%s\n", topic.c_str(),
                  payload.c_str());
  }

  if (topic == topics_.weather || topic == topics_.climate ||
      topic == topics_.boilerPower || topic == topics_.windows ||
      topic == topics_.calendar || topic == topics_.waste ||
      topic == topics_.homeAssistantStatus) {
    state_.diagnostics.mqtt.lastHaStateMs = millis();
  }

  if (topic == topics_.weather) {
    parseWeather(payload);
    return;
  }
  if (topic == topics_.climate) {
    parseClimate(payload);
    return;
  }
  if (topic == topics_.boilerPower) {
    parseBinaryState(payload, true);
    return;
  }
  if (topic == topics_.windows) {
    parseBinaryState(payload, false);
    return;
  }
  if (topic == topics_.calendar) {
    parseCalendar(payload);
    return;
  }
  if (topic == topics_.waste) {
    parseWaste(payload);
    return;
  }
  if (topic != topics_.homeAssistantStatus) {
    return;
  }

  payload.trim();
  if (payload == "online") {
    state_.homeAssistant.homeAssistantOnline = true;
    Serial.println("HA STATUS online");
    startDiscoveryPublish();
    startSnapshotSynchronization();
  } else if (payload == "offline") {
    state_.homeAssistant.homeAssistantOnline = false;
    Serial.println("HA STATUS offline");
  }
}

bool HomeAssistantMqtt::subscribeStateTopics() {
  return mqttClient_.subscribe(topics_.weather, 1) &&
         mqttClient_.subscribe(topics_.climate, 1) &&
         mqttClient_.subscribe(topics_.boilerPower, 1) &&
         mqttClient_.subscribe(topics_.windows, 1) &&
         mqttClient_.subscribe(topics_.calendar, 1) &&
         mqttClient_.subscribe(topics_.waste, 1);
}

void HomeAssistantMqtt::parseWeather(const String& payload) {
  HomeAssistantWeatherState& weather = state_.homeAssistant.weather;
  weather.group.received = true;
  weather.group.parseError = false;
  weather.group.lastUpdateMs = millis();
  weather.temperature.received = true;

  const TextSpan span = trimmedSpan(payload);
  bool valid = !isUnavailableText(span);
  float temperature = 0.0F;
  if (valid) {
    char* end = nullptr;
    temperature = strtof(span.data, &end);
    const char* expectedEnd = span.data + span.length;
    valid = end != span.data && end == expectedEnd && isfinite(temperature);
  }
  if (valid) {
    weather.temperature.value = temperature;
  }
  weather.temperature.valid = valid;
  weather.group.valid = valid;
  Serial.println("HA STATE weather received");
  finishSnapshotIfComplete();
}

void HomeAssistantMqtt::parseClimate(const String& payload) {
  HomeAssistantClimateState& climate = state_.homeAssistant.climate;
  climate.group.received = true;
  climate.group.lastUpdateMs = millis();

  StaticJsonDocument<Config::HomeAssistant::CLIMATE_JSON_SIZE> document;
  const DeserializationError error = deserializeJson(document, payload);
  if (error || !document.is<JsonObject>()) {
    climate.group.valid = false;
    climate.group.parseError = true;
    Serial.println("HA CLIMATE parse error");
    Serial.println("HA STATE climate received");
    finishSnapshotIfComplete();
    return;
  }

  climate.group.valid = true;
  climate.group.parseError = false;
  const JsonObjectConst root = document.as<JsonObjectConst>();

  setJsonText(climate.hvacModeRaw, root["hvac_mode"]);
  climate.hvacMode.received = true;
  climate.hvacMode.value = HvacMode::UNKNOWN;
  climate.hvacMode.valid = false;
  if (climate.hvacModeRaw.valid) {
    const TextSpan raw{climate.hvacModeRaw.value,
                       strlen(climate.hvacModeRaw.value)};
    if (spanEqualsIgnoreCase(raw, "heat")) {
      climate.hvacMode.value = HvacMode::HEAT;
      climate.hvacMode.valid = true;
    } else if (spanEqualsIgnoreCase(raw, "off")) {
      climate.hvacMode.value = HvacMode::OFF;
      climate.hvacMode.valid = true;
    }
  }

  setJsonText(climate.presetModeRaw, root["preset_mode"]);
  climate.presetMode.received = true;
  climate.presetMode.value = PresetMode::UNKNOWN;
  climate.presetMode.valid = false;
  if (climate.presetModeRaw.valid) {
    const TextSpan raw{climate.presetModeRaw.value,
                       strlen(climate.presetModeRaw.value)};
    if (spanEqualsIgnoreCase(raw, "comfort")) {
      climate.presetMode.value = PresetMode::COMFORT;
      climate.presetMode.valid = true;
    } else if (spanEqualsIgnoreCase(raw, "sleep")) {
      climate.presetMode.value = PresetMode::SLEEP;
      climate.presetMode.valid = true;
    } else if (spanEqualsIgnoreCase(raw, "none")) {
      climate.presetMode.value = PresetMode::NONE;
      climate.presetMode.valid = true;
    }
  }

  setJsonText(climate.hvacActionRaw, root["hvac_action"]);
  climate.hvacAction.received = true;
  climate.hvacAction.value = HvacAction::UNKNOWN;
  climate.hvacAction.valid = false;
  if (climate.hvacActionRaw.valid) {
    const TextSpan raw{climate.hvacActionRaw.value,
                       strlen(climate.hvacActionRaw.value)};
    if (spanEqualsIgnoreCase(raw, "heating")) {
      climate.hvacAction.value = HvacAction::HEATING;
      climate.hvacAction.valid = true;
    } else if (spanEqualsIgnoreCase(raw, "idle")) {
      climate.hvacAction.value = HvacAction::IDLE;
      climate.hvacAction.valid = true;
    } else if (spanEqualsIgnoreCase(raw, "off")) {
      climate.hvacAction.value = HvacAction::OFF;
      climate.hvacAction.valid = true;
    }
  }

  float number = 0.0F;
  climate.targetTemperature.received = true;
  if (readJsonNumber(root["target_temperature"], number)) {
    climate.targetTemperature.value = number;
    climate.targetTemperature.valid =
        number >= Config::Boiler::TARGET_MIN_C &&
        number <= Config::Boiler::TARGET_MAX_C;
  } else {
    climate.targetTemperature.valid = false;
  }

  climate.currentTemperature.received = true;
  if (readJsonNumber(root["current_temperature"], number)) {
    climate.currentTemperature.value = number;
    climate.currentTemperature.valid = true;
  } else {
    climate.currentTemperature.valid = false;
  }

  Serial.println("HA STATE climate received");
  finishSnapshotIfComplete();
}

void HomeAssistantMqtt::parseBinaryState(const String& payload,
                                         bool boilerPower) {
  HomeAssistantBinaryState& binary =
      boilerPower ? state_.homeAssistant.boilerPower
                  : state_.homeAssistant.windows;
  binary.group.received = true;
  binary.group.parseError = false;
  binary.group.lastUpdateMs = millis();
  binary.state.received = true;
  binary.state.valid = false;
  binary.state.value = BinaryState::UNKNOWN;

  const TextSpan span = trimmedSpan(payload);
  if (spanEqualsIgnoreCase(span, "on")) {
    binary.state.value = BinaryState::ON;
    binary.state.valid = true;
  } else if (spanEqualsIgnoreCase(span, "off")) {
    binary.state.value = BinaryState::OFF;
    binary.state.valid = true;
  }
  binary.group.valid = binary.state.valid;
  Serial.printf("HA STATE %s received\n",
                boilerPower ? "boiler_power" : "windows");
  finishSnapshotIfComplete();
}

void HomeAssistantMqtt::parseCalendar(const String& payload) {
  HomeAssistantCalendarState& calendar = state_.homeAssistant.calendar;
  calendar.group.received = true;
  calendar.group.lastUpdateMs = millis();

  StaticJsonDocument<Config::HomeAssistant::CALENDAR_JSON_SIZE> document;
  const DeserializationError error = deserializeJson(document, payload);
  if (error || !document.is<JsonObject>()) {
    calendar.group.valid = false;
    calendar.group.parseError = true;
    Serial.println("HA CALENDAR parse error");
    Serial.println("HA STATE calendar received");
    finishSnapshotIfComplete();
    return;
  }

  calendar.group.parseError = false;
  const JsonObjectConst root = document.as<JsonObjectConst>();
  const JsonArrayConst today = root["today"].as<JsonArrayConst>();
  const JsonArrayConst tomorrow = root["tomorrow"].as<JsonArrayConst>();
  calendar.group.valid = !today.isNull() && !tomorrow.isNull();
  for (size_t index = 0; index < 4; ++index) {
    setJsonText(calendar.today[index], today[index]);
    setJsonText(calendar.tomorrow[index], tomorrow[index]);
  }

  Serial.println("HA STATE calendar received");
  finishSnapshotIfComplete();
}

void HomeAssistantMqtt::parseWaste(const String& payload) {
  HomeAssistantWasteState& waste = state_.homeAssistant.waste;
  waste.group.received = true;
  waste.group.lastUpdateMs = millis();

  StaticJsonDocument<Config::HomeAssistant::WASTE_JSON_SIZE> document;
  const DeserializationError error = deserializeJson(document, payload);
  if (error || !document.is<JsonObject>()) {
    waste.group.valid = false;
    waste.group.parseError = true;
    Serial.println("HA WASTE parse error");
    Serial.println("HA STATE waste received");
    finishSnapshotIfComplete();
    return;
  }

  waste.group.valid = true;
  waste.group.parseError = false;
  const JsonObjectConst root = document.as<JsonObjectConst>();
  setJsonText(waste.mixed, root["mixed"]);
  setJsonText(waste.paper, root["paper"]);
  setJsonText(waste.glass, root["glass"]);
  setJsonText(waste.plastic, root["plastic"]);

  Serial.println("HA STATE waste received");
  finishSnapshotIfComplete();
}

void HomeAssistantMqtt::startSnapshotSynchronization() {
  state_.homeAssistant.weather.group.received = false;
  state_.homeAssistant.climate.group.received = false;
  state_.homeAssistant.boilerPower.group.received = false;
  state_.homeAssistant.windows.group.received = false;
  state_.homeAssistant.calendar.group.received = false;
  state_.homeAssistant.waste.group.received = false;
  state_.homeAssistant.snapshotComplete = false;
  state_.homeAssistant.snapshotReceivedGroups = 0;
  snapshotSynchronizationActive_ = true;
  snapshotLastRequestMs_ = millis();
  requestSnapshot();
}

void HomeAssistantMqtt::serviceSnapshotSynchronization(uint32_t now) {
  if (!snapshotSynchronizationActive_ || snapshotRequestPending_) {
    return;
  }
  if (static_cast<uint32_t>(now - snapshotLastRequestMs_) <
      Config::HomeAssistant::SNAPSHOT_RETRY_INTERVAL_MS) {
    return;
  }

  Serial.printf("HA SNAPSHOT incomplete %u/%u\n", receivedGroupCount(),
                HOME_ASSISTANT_GROUP_COUNT);
  requestSnapshot();
}

uint8_t HomeAssistantMqtt::receivedGroupCount() const {
  uint8_t count = 0;
  count += state_.homeAssistant.weather.group.received ? 1U : 0U;
  count += state_.homeAssistant.climate.group.received ? 1U : 0U;
  count += state_.homeAssistant.boilerPower.group.received ? 1U : 0U;
  count += state_.homeAssistant.windows.group.received ? 1U : 0U;
  count += state_.homeAssistant.calendar.group.received ? 1U : 0U;
  count += state_.homeAssistant.waste.group.received ? 1U : 0U;
  return count;
}

void HomeAssistantMqtt::finishSnapshotIfComplete() {
  const uint8_t received = receivedGroupCount();
  state_.homeAssistant.snapshotReceivedGroups = received;
  if (snapshotSynchronizationActive_ &&
      received == HOME_ASSISTANT_GROUP_COUNT) {
    snapshotSynchronizationActive_ = false;
    state_.homeAssistant.snapshotComplete = true;
    state_.homeAssistant.snapshotLastCompleteMs = millis();
    state_.diagnostics.mqtt.lastSnapshotCompleteMs =
        state_.homeAssistant.snapshotLastCompleteMs;
    Serial.println("HA SNAPSHOT COMPLETE");
  }
}

void HomeAssistantMqtt::servicePendingPublications() {
  if (!mqttClient_.connected()) {
    return;
  }

  if (snapshotRequestPending_) {
    if (publishMessage(topics_.snapshotRequest, "request", false, 1)) {
      snapshotRequestPending_ = false;
      snapshotLastRequestMs_ = millis();
      Serial.println("MQTT SNAPSHOT REQUEST");
    }
    return;
  }

  if (discoveryPublishActive_) {
    if (publishDiscoveryEntity(discoveryIndex_)) {
      ++state_.diagnostics.mqtt.discoveryPublishCount;
      ++discoveryIndex_;
      if (discoveryIndex_ >= DISCOVERY_ENTITY_COUNT) {
        discoveryPublishActive_ = false;
        Serial.println("MQTT DISCOVERY published");
      }
    }
    return;
  }

  if (diagnosticSnapshotActive_) {
    if (publishDiagnostic(diagnosticIndex_)) {
      ++diagnosticIndex_;
      if (diagnosticIndex_ >= DIAGNOSTIC_ENTITY_COUNT) {
        diagnosticSnapshotActive_ = false;
      }
    }
    return;
  }

  if (periodicDiagnosticsActive_) {
    if (publishPeriodicDiagnostic(periodicDiagnosticIndex_)) {
      ++periodicDiagnosticIndex_;
      if (periodicDiagnosticIndex_ >= PERIODIC_DIAGNOSTIC_COUNT) {
        periodicDiagnosticsActive_ = false;
      }
    }
    return;
  }

  const bool yoRadioOnline =
      state_.radio.connection == RadioConnectionState::ONLINE &&
      state_.radio.firstDataReceived;
  if (!cache_.yoRadioValid ||
      cache_.yoRadioOnline != yoRadioOnline) {
    publishYoRadioStatus();
    return;
  }

  const bool nextionOnline = state_.ui.nextionSynchronized;
  if (!cache_.nextionValid ||
      cache_.nextionOnline != nextionOnline) {
    publishNextionStatus();
    return;
  }

  const int32_t rssiDifference =
      state_.network.rssi >= cache_.wifiRssi
          ? state_.network.rssi - cache_.wifiRssi
          : cache_.wifiRssi - state_.network.rssi;
  if (!cache_.wifiRssiValid ||
      rssiDifference >= Config::Mqtt::RSSI_CHANGE_THRESHOLD_DB) {
    publishWifiRssi();
  }
}

bool HomeAssistantMqtt::publishMessage(const char* topic,
                                       const char* payload, bool retained,
                                       int qos) {
  if (!mqttClient_.connected()) {
    return false;
  }
  if (Config::Logging::DEBUG_MQTT) {
    Serial.printf("MQTT TX topic=%s payload=%s\n", topic, payload);
  }
  const bool published = mqttClient_.publish(topic, payload, retained, qos);
  state_.diagnostics.mqtt.lastError =
      static_cast<int32_t>(mqttClient_.lastError());
  state_.diagnostics.mqtt.lastReturnCode =
      static_cast<int32_t>(mqttClient_.returnCode());
  return published;
}

bool HomeAssistantMqtt::publishCommand(const char* topic,
                                       const char* payload) {
  if (phase_ != Phase::CONNECTED ||
      !state_.homeAssistant.mqttConnected ||
      !mqttClient_.connected()) {
    return false;
  }
  // Komendy nigdy nie sa retained; QoS 1 zapewnia dostarczenie do brokera.
  return publishMessage(topic, payload, false, 1);
}

bool HomeAssistantMqtt::publishDiscoveryEntity(uint8_t index) {
  const char* component = nullptr;
  const char* key = nullptr;
  const char* name = nullptr;
  const char* stateTopic = nullptr;

  switch (index) {
    case 0:
      component = "binary_sensor";
      key = "status";
      name = "Status panelu";
      stateTopic = topics_.status;
      break;
    case 1:
      component = "binary_sensor";
      key = "yoradio";
      name = "Status yoRadio";
      stateTopic = topics_.yoRadio;
      break;
    case 2:
      component = "binary_sensor";
      key = "nextion";
      name = "Status Nextion";
      stateTopic = topics_.nextion;
      break;
    case 3:
      component = "sensor";
      key = "wifi_rssi";
      name = "WiFi RSSI";
      stateTopic = topics_.wifiRssi;
      break;
    case 4:
      component = "sensor";
      key = "uptime";
      name = "Uptime";
      stateTopic = topics_.uptime;
      break;
    case 5:
      component = "sensor";
      key = "firmware";
      name = "Firmware";
      stateTopic = topics_.firmware;
      break;
    default:
      return false;
  }

  char discoveryTopic[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
  char uniqueId[96]{};
  const int topicLength =
      snprintf(discoveryTopic, sizeof(discoveryTopic), "%s/%s/%s_%s/config",
               config_.haDiscoveryPrefix(), component, objectId_, key);
  const int uniqueIdLength =
      snprintf(uniqueId, sizeof(uniqueId), "%s_%s", objectId_, key);
  if (topicLength <= 0 ||
      static_cast<size_t>(topicLength) >= sizeof(discoveryTopic) ||
      uniqueIdLength <= 0 ||
      static_cast<size_t>(uniqueIdLength) >= sizeof(uniqueId)) {
    return false;
  }

  StaticJsonDocument<Config::Mqtt::DISCOVERY_JSON_SIZE> document;
  document["name"] = name;
  document["unique_id"] = uniqueId;
  document["state_topic"] = stateTopic;
  document["entity_category"] = "diagnostic";

  if (index <= 2) {
    document["payload_on"] = "online";
    document["payload_off"] = "offline";
    document["device_class"] = "connectivity";
  } else if (index == 3) {
    document["device_class"] = "signal_strength";
    document["unit_of_measurement"] = "dBm";
    document["state_class"] = "measurement";
  } else if (index == 4) {
    document["device_class"] = "duration";
    document["unit_of_measurement"] = "s";
  }

  if (index != 0) {
    document["availability_topic"] = topics_.status;
    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";
  }

  JsonObject device = document.createNestedObject("device");
  device["name"] = config_.deviceName();
  device["manufacturer"] = Config::Identity::MANUFACTURER;
  device["model"] = Config::Identity::MODEL;
  device["sw_version"] = FirmwareInfo::VERSION;
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(deviceIdentifier_);

  char payload[Config::Mqtt::DISCOVERY_JSON_SIZE]{};
  const size_t payloadLength =
      serializeJson(document, payload, sizeof(payload));
  if (payloadLength == 0 || payloadLength >= sizeof(payload)) {
    return false;
  }
  return publishMessage(discoveryTopic, payload, true, 1);
}

bool HomeAssistantMqtt::publishDiagnostic(uint8_t index) {
  switch (index) {
    case 0:
      return publishYoRadioStatus();
    case 1:
      return publishNextionStatus();
    case 2:
      return publishWifiRssi();
    case 3:
      return publishUptime();
    case 4:
      return publishFirmware();
    default:
      return false;
  }
}

bool HomeAssistantMqtt::publishPeriodicDiagnostic(uint8_t index) {
  if (index == 0) {
    return publishWifiRssi();
  }
  if (index == 1) {
    return publishUptime();
  }
  return false;
}

bool HomeAssistantMqtt::publishYoRadioStatus() {
  const bool online =
      state_.radio.connection == RadioConnectionState::ONLINE &&
      state_.radio.firstDataReceived;
  if (cache_.yoRadioValid && cache_.yoRadioOnline == online) {
    return true;
  }
  if (!publishMessage(topics_.yoRadio, online ? "online" : "offline", true,
                      1)) {
    return false;
  }
  cache_.yoRadioOnline = online;
  cache_.yoRadioValid = true;
  return true;
}

bool HomeAssistantMqtt::publishNextionStatus() {
  const bool online = state_.ui.nextionSynchronized;
  if (cache_.nextionValid && cache_.nextionOnline == online) {
    return true;
  }
  if (!publishMessage(topics_.nextion, online ? "online" : "offline", true,
                      1)) {
    return false;
  }
  cache_.nextionOnline = online;
  cache_.nextionValid = true;
  return true;
}

bool HomeAssistantMqtt::publishWifiRssi() {
  const int32_t rssi = state_.network.rssi;
  if (cache_.wifiRssiValid && cache_.wifiRssi == rssi) {
    return true;
  }

  char payload[16]{};
  snprintf(payload, sizeof(payload), "%ld", static_cast<long>(rssi));
  if (!publishMessage(topics_.wifiRssi, payload, true, 1)) {
    return false;
  }
  cache_.wifiRssi = rssi;
  cache_.wifiRssiValid = true;
  return true;
}

bool HomeAssistantMqtt::publishUptime() {
  const uint32_t uptimeSeconds = millis() / 1000U;
  if (cache_.uptimeValid && cache_.uptimeSeconds == uptimeSeconds) {
    return true;
  }

  char payload[16]{};
  snprintf(payload, sizeof(payload), "%lu",
           static_cast<unsigned long>(uptimeSeconds));
  if (!publishMessage(topics_.uptime, payload, true, 1)) {
    return false;
  }
  cache_.uptimeSeconds = uptimeSeconds;
  cache_.uptimeValid = true;
  return true;
}

bool HomeAssistantMqtt::publishFirmware() {
  if (cache_.firmwareValid) {
    return true;
  }
  if (!publishMessage(topics_.firmware, FirmwareInfo::VERSION, true, 1)) {
    return false;
  }
  cache_.firmwareValid = true;
  return true;
}

void HomeAssistantMqtt::invalidatePublishCache() {
  cache_ = DiagnosticCache{};
}

void HomeAssistantMqtt::startDiscoveryPublish() {
  if (!config_.haDiscoveryEnabled()) {
    return;
  }
  discoveryIndex_ = 0;
  discoveryPublishActive_ = true;
}

void HomeAssistantMqtt::startDiagnosticSnapshot() {
  diagnosticIndex_ = 0;
  diagnosticSnapshotActive_ = true;
}
