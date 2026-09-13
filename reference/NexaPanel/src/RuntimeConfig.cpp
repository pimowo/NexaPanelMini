#include "RuntimeConfig.h"

#include <IPAddress.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

namespace {

constexpr char NVS_NAMESPACE[] = "kpanel_cfg";

template <size_t Size>
void copyText(char (&target)[Size], const char* source) {
  snprintf(target, Size, "%s", source != nullptr ? source : "");
}

bool hasLength(const char* text, size_t minimum, size_t maximum) {
  if (text == nullptr) {
    return false;
  }
  const size_t length = strlen(text);
  return length >= minimum && length <= maximum;
}

bool isPrintableWithoutWhitespace(const char* text, size_t minimum,
                                  size_t maximum) {
  if (!hasLength(text, minimum, maximum)) {
    return false;
  }
  for (const uint8_t* cursor = reinterpret_cast<const uint8_t*>(text);
       *cursor != 0; ++cursor) {
    if (*cursor <= 0x20U || *cursor == 0x7FU) {
      return false;
    }
  }
  return true;
}

bool isPrintableText(const char* text, size_t minimum, size_t maximum) {
  if (!hasLength(text, minimum, maximum)) {
    return false;
  }
  for (const uint8_t* cursor = reinterpret_cast<const uint8_t*>(text);
       *cursor != 0; ++cursor) {
    if (*cursor < 0x20U || *cursor == 0x7FU) {
      return false;
    }
  }
  return true;
}

bool isValidHostname(const char* hostname) {
  if (!hasLength(hostname, 1, 32) || hostname[0] == '-' ||
      hostname[strlen(hostname) - 1] == '-') {
    return false;
  }
  for (const char* cursor = hostname; *cursor != '\0'; ++cursor) {
    const bool valid = (*cursor >= 'a' && *cursor <= 'z') ||
                       (*cursor >= 'A' && *cursor <= 'Z') ||
                       (*cursor >= '0' && *cursor <= '9') || *cursor == '-';
    if (!valid) {
      return false;
    }
  }
  return true;
}

bool isValidWifiPassword(const char* password) {
  const size_t length = password != nullptr ? strlen(password) : 0;
  return length == 0 || (length >= 8 && length <= 63);
}

bool isValidIpv4(const char* text, bool optional) {
  if (text == nullptr || text[0] == '\0') {
    return optional;
  }
  IPAddress address;
  return address.fromString(text);
}

bool isValidStaticNetwork(const RuntimeConfig::PublicValues& values) {
  const bool required = !values.wifiDhcp;
  return isValidIpv4(values.wifiStaticIp, !required) &&
         isValidIpv4(values.wifiGateway, !required) &&
         isValidIpv4(values.wifiSubnet, !required) &&
         isValidIpv4(values.wifiDns1, true) &&
         isValidIpv4(values.wifiDns2, true);
}

}  // namespace

RuntimeConfig::RuntimeConfig() {
  setDefaults(values_);
}

bool RuntimeConfig::load() {
  Serial.println("CONFIG START");
  Values defaults;
  setDefaults(defaults);
  values_ = defaults;
  source_ = Source::DEFAULTS;

  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    Serial.println("CONFIG NVS niedostępne, używam defaults");
    return true;
  }

  if (!preferences.isKey("schema")) {
    preferences.end();
    Serial.println("CONFIG NVS puste, używam defaults");
    return true;
  }

  const uint16_t schema = preferences.getUShort("schema", 0);
  if (schema != SCHEMA_VERSION) {
    preferences.end();
    Serial.printf("CONFIG schema NVS=%u nieobsługiwane, używam defaults\n",
                  schema);
    return true;
  }

  bool profileComplete = true;
  auto loadText = [&](const char* key, char* target, size_t targetSize,
                      const char* fallback) {
    if (!preferences.isKey(key)) {
      logInvalidField(key);
      profileComplete = false;
      snprintf(target, targetSize, "%s", fallback);
      return;
    }
    const String loaded = preferences.getString(key, fallback);
    if (loaded.length() >= targetSize) {
      logInvalidField(key);
      profileComplete = false;
      snprintf(target, targetSize, "%s", fallback);
      return;
    }
    snprintf(target, targetSize, "%s", loaded.c_str());
  };
  auto loadBool = [&](const char* key, bool fallback) {
    if (!preferences.isKey(key)) {
      logInvalidField(key);
      profileComplete = false;
      return fallback;
    }
    return preferences.getBool(key, fallback);
  };
  auto loadUShort = [&](const char* key, uint16_t fallback) {
    if (!preferences.isKey(key)) {
      logInvalidField(key);
      profileComplete = false;
      return fallback;
    }
    return preferences.getUShort(key, fallback);
  };
  auto loadULong = [&](const char* key, uint32_t fallback) {
    if (!preferences.isKey(key)) {
      logInvalidField(key);
      profileComplete = false;
      return fallback;
    }
    return preferences.getULong(key, fallback);
  };

  loadText("dev_name", values_.deviceName, sizeof(values_.deviceName),
           defaults.deviceName);
  loadText("hostname", values_.hostname, sizeof(values_.hostname),
           defaults.hostname);
  loadText("wifi_ssid", values_.wifiSsid, sizeof(values_.wifiSsid),
           defaults.wifiSsid);
  loadText("wifi_pass", values_.wifiPassword, sizeof(values_.wifiPassword),
           defaults.wifiPassword);
  values_.wifiDhcp = loadBool("wifi_dhcp", defaults.wifiDhcp);
  loadText("wifi_ip", values_.wifiStaticIp, sizeof(values_.wifiStaticIp),
           defaults.wifiStaticIp);
  loadText("wifi_gw", values_.wifiGateway, sizeof(values_.wifiGateway),
           defaults.wifiGateway);
  loadText("wifi_mask", values_.wifiSubnet, sizeof(values_.wifiSubnet),
           defaults.wifiSubnet);
  loadText("wifi_dns1", values_.wifiDns1, sizeof(values_.wifiDns1),
           defaults.wifiDns1);
  loadText("wifi_dns2", values_.wifiDns2, sizeof(values_.wifiDns2),
           defaults.wifiDns2);
  loadText("mqtt_host", values_.mqttHost, sizeof(values_.mqttHost),
           defaults.mqttHost);
  values_.mqttPort = loadUShort("mqtt_port", defaults.mqttPort);
  loadText("mqtt_user", values_.mqttUsername, sizeof(values_.mqttUsername),
           defaults.mqttUsername);
  loadText("mqtt_pass", values_.mqttPassword, sizeof(values_.mqttPassword),
           defaults.mqttPassword);
  loadText("mqtt_client", values_.mqttClientId,
           sizeof(values_.mqttClientId), defaults.mqttClientId);
  loadText("mqtt_topic", values_.mqttBaseTopic,
           sizeof(values_.mqttBaseTopic), defaults.mqttBaseTopic);
  loadText("ha_prefix", values_.haDiscoveryPrefix,
           sizeof(values_.haDiscoveryPrefix), defaults.haDiscoveryPrefix);
  values_.haDiscoveryEnabled =
      loadBool("ha_disc", defaults.haDiscoveryEnabled);
  loadText("yr_host", values_.yoRadioHost, sizeof(values_.yoRadioHost),
           defaults.yoRadioHost);
  values_.yoRadioPort = loadUShort("yr_port", defaults.yoRadioPort);
  loadText("yr_path", values_.yoRadioPath, sizeof(values_.yoRadioPath),
           defaults.yoRadioPath);
  loadText("tz_name", values_.timezoneName, sizeof(values_.timezoneName),
           defaults.timezoneName);
  loadText("tz_rule", values_.timezoneRule, sizeof(values_.timezoneRule),
           defaults.timezoneRule);
  values_.uiPageTimeoutMs =
      loadULong("ui_timeout", defaults.uiPageTimeoutMs);
  const bool checksumAvailable = preferences.isKey("crc");
  const uint32_t storedChecksum = preferences.getULong("crc", 0);
  preferences.end();

  if (!profileComplete || !checksumAvailable ||
      storedChecksum != checksum(values_)) {
    values_ = defaults;
    Serial.println("CONFIG WARNING: profil NVS niekompletny lub uszkodzony");
    return true;
  }

  validateAndRepair(values_, defaults);
  source_ = Source::NVS;
  return true;
}

bool RuntimeConfig::save() {
  if (!validate(values_)) {
    Serial.println("CONFIG SAVE ERROR: walidacja");
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) {
    Serial.println("CONFIG SAVE ERROR: NVS");
    return false;
  }

  preferences.putString("dev_name", values_.deviceName);
  preferences.putString("hostname", values_.hostname);
  preferences.putString("wifi_ssid", values_.wifiSsid);
  preferences.putString("wifi_pass", values_.wifiPassword);
  preferences.putBool("wifi_dhcp", values_.wifiDhcp);
  preferences.putString("wifi_ip", values_.wifiStaticIp);
  preferences.putString("wifi_gw", values_.wifiGateway);
  preferences.putString("wifi_mask", values_.wifiSubnet);
  preferences.putString("wifi_dns1", values_.wifiDns1);
  preferences.putString("wifi_dns2", values_.wifiDns2);
  preferences.putString("mqtt_host", values_.mqttHost);
  preferences.putUShort("mqtt_port", values_.mqttPort);
  preferences.putString("mqtt_user", values_.mqttUsername);
  preferences.putString("mqtt_pass", values_.mqttPassword);
  preferences.putString("mqtt_client", values_.mqttClientId);
  preferences.putString("mqtt_topic", values_.mqttBaseTopic);
  preferences.putString("ha_prefix", values_.haDiscoveryPrefix);
  preferences.putBool("ha_disc", values_.haDiscoveryEnabled);
  preferences.putString("yr_host", values_.yoRadioHost);
  preferences.putUShort("yr_port", values_.yoRadioPort);
  preferences.putString("yr_path", values_.yoRadioPath);
  preferences.putString("tz_name", values_.timezoneName);
  preferences.putString("tz_rule", values_.timezoneRule);
  preferences.putULong("ui_timeout", values_.uiPageTimeoutMs);
  const bool checksumSaved =
      preferences.putULong("crc", checksum(values_)) == sizeof(uint32_t);
  const bool schemaSaved =
      preferences.putUShort("schema", SCHEMA_VERSION) == sizeof(uint16_t);
  preferences.end();

  if (!checksumSaved || !schemaSaved) {
    Serial.println("CONFIG SAVE ERROR: integralność");
    return false;
  }
  source_ = Source::NVS;
  Serial.println("CONFIG SAVED");
  return true;
}

bool RuntimeConfig::resetToDefaults() {
  Values defaults;
  setDefaults(defaults);

  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) {
    Serial.println("CONFIG RESET ERROR: NVS");
    return false;
  }
  const bool cleared = preferences.clear();
  preferences.end();
  if (!cleared) {
    Serial.println("CONFIG RESET ERROR: clear");
    return false;
  }

  values_ = defaults;
  source_ = Source::DEFAULTS;
  Serial.println("CONFIG RESET TO DEFAULTS");
  return true;
}

bool RuntimeConfig::apply(const PublicValues& values,
                          const char* wifiPasswordInput,
                          const char* mqttPasswordInput) {
  return applyChecked(values, wifiPasswordInput, mqttPasswordInput) ==
         ApplyResult::OK;
}

RuntimeConfig::ApplyResult RuntimeConfig::applyChecked(
    const PublicValues& values, const char* wifiPasswordInput,
    const char* mqttPasswordInput) {
  Values candidate = values_;
  static_cast<PublicValues&>(candidate) = values;
  if (wifiPasswordInput != nullptr && wifiPasswordInput[0] != '\0') {
    if (strlen(wifiPasswordInput) > 63) {
      return ApplyResult::VALIDATION_ERROR;
    }
    copyText(candidate.wifiPassword, wifiPasswordInput);
  }
  if (mqttPasswordInput != nullptr && mqttPasswordInput[0] != '\0') {
    if (strlen(mqttPasswordInput) > 64) {
      return ApplyResult::VALIDATION_ERROR;
    }
    copyText(candidate.mqttPassword, mqttPasswordInput);
  }
  if (!validate(candidate)) {
    return ApplyResult::VALIDATION_ERROR;
  }

  const Values previous = values_;
  const Source previousSource = source_;
  values_ = candidate;
  if (!save()) {
    values_ = previous;
    source_ = previousSource;
    return ApplyResult::STORAGE_ERROR;
  }
  return ApplyResult::OK;
}

void RuntimeConfig::copyPublic(PublicValues& output) const {
  output = static_cast<const PublicValues&>(values_);
  output.wifiPasswordConfigured = values_.wifiPassword[0] != '\0';
  output.mqttPasswordConfigured = values_.mqttPassword[0] != '\0';
}

RuntimeConfig::Source RuntimeConfig::source() const {
  return source_;
}

const char* RuntimeConfig::sourceName() const {
  return source_ == Source::NVS ? "NVS" : "DEFAULTS";
}

const char* RuntimeConfig::deviceName() const { return values_.deviceName; }
const char* RuntimeConfig::hostname() const { return values_.hostname; }
const char* RuntimeConfig::wifiSsid() const { return values_.wifiSsid; }
const char* RuntimeConfig::wifiPassword() const {
  return values_.wifiPassword;
}
bool RuntimeConfig::wifiDhcp() const { return values_.wifiDhcp; }
const char* RuntimeConfig::wifiStaticIp() const {
  return values_.wifiStaticIp;
}
const char* RuntimeConfig::wifiGateway() const {
  return values_.wifiGateway;
}
const char* RuntimeConfig::wifiSubnet() const { return values_.wifiSubnet; }
const char* RuntimeConfig::wifiDns1() const { return values_.wifiDns1; }
const char* RuntimeConfig::wifiDns2() const { return values_.wifiDns2; }
const char* RuntimeConfig::mqttHost() const { return values_.mqttHost; }
uint16_t RuntimeConfig::mqttPort() const { return values_.mqttPort; }
const char* RuntimeConfig::mqttUsername() const {
  return values_.mqttUsername;
}
const char* RuntimeConfig::mqttPassword() const {
  return values_.mqttPassword;
}
const char* RuntimeConfig::mqttClientId() const {
  return values_.mqttClientId;
}
const char* RuntimeConfig::mqttBaseTopic() const {
  return values_.mqttBaseTopic;
}
const char* RuntimeConfig::haDiscoveryPrefix() const {
  return values_.haDiscoveryPrefix;
}
bool RuntimeConfig::haDiscoveryEnabled() const {
  return values_.haDiscoveryEnabled;
}
const char* RuntimeConfig::yoRadioHost() const {
  return values_.yoRadioHost;
}
uint16_t RuntimeConfig::yoRadioPort() const {
  return values_.yoRadioPort;
}
const char* RuntimeConfig::yoRadioPath() const {
  return values_.yoRadioPath;
}
const char* RuntimeConfig::timezoneName() const {
  return values_.timezoneName;
}
const char* RuntimeConfig::timezoneRule() const {
  return values_.timezoneRule;
}
uint32_t RuntimeConfig::uiPageTimeoutMs() const {
  return values_.uiPageTimeoutMs;
}

void RuntimeConfig::setDefaults(Values& values) {
  values = Values{};
  copyText(values.deviceName, Config::Identity::DEVICE_NAME);
  copyText(values.hostname, Config::Identity::HOSTNAME);
  copyText(values.wifiSsid, Config::Wifi::WIFI_SSID);
  copyText(values.wifiPassword, Config::Wifi::WIFI_PASSWORD);
  values.wifiDhcp = Config::Wifi::DHCP_ENABLED;
  copyText(values.wifiStaticIp, Config::Wifi::STATIC_IP);
  copyText(values.wifiGateway, Config::Wifi::GATEWAY);
  copyText(values.wifiSubnet, Config::Wifi::SUBNET);
  copyText(values.wifiDns1, Config::Wifi::DNS1);
  copyText(values.wifiDns2, Config::Wifi::DNS2);
  copyText(values.mqttHost, Config::Mqtt::BROKER_HOST);
  values.mqttPort = Config::Mqtt::BROKER_PORT;
  copyText(values.mqttUsername, Config::Mqtt::USERNAME);
  copyText(values.mqttPassword, Config::Mqtt::PASSWORD);
  copyText(values.mqttClientId, Config::Mqtt::CLIENT_ID);
  copyText(values.mqttBaseTopic, Config::Mqtt::BASE_TOPIC);
  copyText(values.haDiscoveryPrefix, Config::Mqtt::DISCOVERY_PREFIX);
  values.haDiscoveryEnabled = Config::Mqtt::DISCOVERY_ENABLED;
  copyText(values.yoRadioHost, Config::YoRadio::HOST);
  values.yoRadioPort = Config::YoRadio::PORT;
  copyText(values.yoRadioPath, Config::YoRadio::PATH);
  copyText(values.timezoneName, Config::Ntp::TIMEZONE_NAME);
  copyText(values.timezoneRule, Config::Ntp::TIMEZONE_RULE);
  values.uiPageTimeoutMs = Config::Ui::UI_PAGE_TIMEOUT_MS;
}

bool RuntimeConfig::validate(const Values& values) {
  return isPrintableText(values.deviceName, 1, 48) &&
         isValidHostname(values.hostname) &&
         isPrintableText(values.wifiSsid, 1, 32) &&
         isValidWifiPassword(values.wifiPassword) &&
         isValidStaticNetwork(values) &&
         isPrintableWithoutWhitespace(values.mqttHost, 1, 64) &&
         values.mqttPort > 0 &&
         isPrintableText(values.mqttUsername, 0, 32) &&
         hasLength(values.mqttPassword, 0, 64) &&
         isPrintableWithoutWhitespace(values.mqttClientId, 1, 64) &&
         isPrintableWithoutWhitespace(values.mqttBaseTopic, 1, 64) &&
         isPrintableWithoutWhitespace(values.haDiscoveryPrefix, 1, 64) &&
         isPrintableWithoutWhitespace(values.yoRadioHost, 1, 64) &&
         values.yoRadioPort > 0 &&
         isPrintableWithoutWhitespace(values.yoRadioPath, 1, 64) &&
         values.yoRadioPath[0] == '/' &&
         isPrintableWithoutWhitespace(values.timezoneName, 1, 32) &&
         isPrintableWithoutWhitespace(values.timezoneRule, 1, 63) &&
         values.uiPageTimeoutMs >= 5000U &&
         values.uiPageTimeoutMs <= 300000U;
}

bool RuntimeConfig::validateAndRepair(Values& values,
                                      const Values& defaults) {
  bool repaired = false;
#define REPAIR_TEXT(field, condition, key) \
  if (!(condition)) {                         \
    copyText(values.field, defaults.field);  \
    logInvalidField(key);                    \
    repaired = true;                         \
  }

  REPAIR_TEXT(deviceName, isPrintableText(values.deviceName, 1, 48),
              "dev_name");
  REPAIR_TEXT(hostname, isValidHostname(values.hostname), "hostname");
  REPAIR_TEXT(wifiSsid, isPrintableText(values.wifiSsid, 1, 32),
              "wifi_ssid");
  REPAIR_TEXT(wifiPassword, isValidWifiPassword(values.wifiPassword),
              "wifi_pass");
  REPAIR_TEXT(mqttHost,
              isPrintableWithoutWhitespace(values.mqttHost, 1, 64),
              "mqtt_host");
  if (values.mqttPort == 0) {
    values.mqttPort = defaults.mqttPort;
    logInvalidField("mqtt_port");
    repaired = true;
  }
  REPAIR_TEXT(mqttUsername,
              isPrintableText(values.mqttUsername, 0, 32), "mqtt_user");
  REPAIR_TEXT(mqttPassword, hasLength(values.mqttPassword, 0, 64),
              "mqtt_pass");
  REPAIR_TEXT(mqttClientId,
              isPrintableWithoutWhitespace(values.mqttClientId, 1, 64),
              "mqtt_client");
  REPAIR_TEXT(mqttBaseTopic,
              isPrintableWithoutWhitespace(values.mqttBaseTopic, 1, 64),
              "mqtt_topic");
  REPAIR_TEXT(haDiscoveryPrefix,
              isPrintableWithoutWhitespace(values.haDiscoveryPrefix, 1, 64),
              "ha_prefix");
  REPAIR_TEXT(yoRadioHost,
              isPrintableWithoutWhitespace(values.yoRadioHost, 1, 64),
              "yr_host");
  if (values.yoRadioPort == 0) {
    values.yoRadioPort = defaults.yoRadioPort;
    logInvalidField("yr_port");
    repaired = true;
  }
  REPAIR_TEXT(yoRadioPath,
              isPrintableWithoutWhitespace(values.yoRadioPath, 1, 64) &&
                  values.yoRadioPath[0] == '/',
              "yr_path");
  REPAIR_TEXT(timezoneName,
              isPrintableWithoutWhitespace(values.timezoneName, 1, 32),
              "tz_name");
  REPAIR_TEXT(timezoneRule,
              isPrintableWithoutWhitespace(values.timezoneRule, 1, 63),
              "tz_rule");
#undef REPAIR_TEXT

  if (!isValidStaticNetwork(values)) {
    values.wifiDhcp = defaults.wifiDhcp;
    copyText(values.wifiStaticIp, defaults.wifiStaticIp);
    copyText(values.wifiGateway, defaults.wifiGateway);
    copyText(values.wifiSubnet, defaults.wifiSubnet);
    copyText(values.wifiDns1, defaults.wifiDns1);
    copyText(values.wifiDns2, defaults.wifiDns2);
    logInvalidField("wifi_ipv4");
    repaired = true;
  }
  if (values.uiPageTimeoutMs < 5000U ||
      values.uiPageTimeoutMs > 300000U) {
    values.uiPageTimeoutMs = defaults.uiPageTimeoutMs;
    logInvalidField("ui_timeout");
    repaired = true;
  }
  return !repaired;
}

uint32_t RuntimeConfig::checksum(const Values& values) {
  uint32_t hash = 2166136261UL;
  auto addBytes = [&](const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t index = 0; index < length; ++index) {
      hash ^= bytes[index];
      hash *= 16777619UL;
    }
  };
  auto addText = [&](const char* text) {
    addBytes(text, strlen(text) + 1U);
  };

  addText(values.deviceName);
  addText(values.hostname);
  addText(values.wifiSsid);
  addText(values.wifiPassword);
  addBytes(&values.wifiDhcp, sizeof(values.wifiDhcp));
  addText(values.wifiStaticIp);
  addText(values.wifiGateway);
  addText(values.wifiSubnet);
  addText(values.wifiDns1);
  addText(values.wifiDns2);
  addText(values.mqttHost);
  addBytes(&values.mqttPort, sizeof(values.mqttPort));
  addText(values.mqttUsername);
  addText(values.mqttPassword);
  addText(values.mqttClientId);
  addText(values.mqttBaseTopic);
  addText(values.haDiscoveryPrefix);
  addBytes(&values.haDiscoveryEnabled,
           sizeof(values.haDiscoveryEnabled));
  addText(values.yoRadioHost);
  addBytes(&values.yoRadioPort, sizeof(values.yoRadioPort));
  addText(values.yoRadioPath);
  addText(values.timezoneName);
  addText(values.timezoneRule);
  addBytes(&values.uiPageTimeoutMs, sizeof(values.uiPageTimeoutMs));
  return hash;
}

void RuntimeConfig::logInvalidField(const char* key) {
  Serial.printf("CONFIG WARNING: %s -> default\n", key);
}
