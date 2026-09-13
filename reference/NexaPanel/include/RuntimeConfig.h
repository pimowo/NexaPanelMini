#pragma once

#include <Arduino.h>

class RuntimeConfig {
 public:
  static constexpr uint16_t SCHEMA_VERSION = 1;

  enum class Source : uint8_t {
    DEFAULTS,
    NVS,
  };

  enum class ApplyResult : uint8_t {
    OK,
    VALIDATION_ERROR,
    STORAGE_ERROR,
  };

  struct PublicValues {
    char deviceName[49]{};
    char hostname[33]{};
    char wifiSsid[33]{};
    bool wifiPasswordConfigured{false};
    bool wifiDhcp{true};
    char wifiStaticIp[16]{};
    char wifiGateway[16]{};
    char wifiSubnet[16]{};
    char wifiDns1[16]{};
    char wifiDns2[16]{};
    char mqttHost[65]{};
    uint16_t mqttPort{1883};
    char mqttUsername[33]{};
    bool mqttPasswordConfigured{false};
    char mqttClientId[65]{};
    char mqttBaseTopic[65]{};
    char haDiscoveryPrefix[65]{};
    bool haDiscoveryEnabled{true};
    char yoRadioHost[65]{};
    uint16_t yoRadioPort{80};
    char yoRadioPath[65]{};
    char timezoneName[33]{};
    char timezoneRule[64]{};
    uint32_t uiPageTimeoutMs{10000};
  };

  RuntimeConfig();

  bool load();
  bool save();
  bool resetToDefaults();
  bool apply(const PublicValues& values, const char* wifiPasswordInput,
             const char* mqttPasswordInput);
  ApplyResult applyChecked(const PublicValues& values,
                           const char* wifiPasswordInput,
                           const char* mqttPasswordInput);
  void copyPublic(PublicValues& output) const;

  Source source() const;
  const char* sourceName() const;

  const char* deviceName() const;
  const char* hostname() const;
  const char* wifiSsid() const;
  const char* wifiPassword() const;
  bool wifiDhcp() const;
  const char* wifiStaticIp() const;
  const char* wifiGateway() const;
  const char* wifiSubnet() const;
  const char* wifiDns1() const;
  const char* wifiDns2() const;
  const char* mqttHost() const;
  uint16_t mqttPort() const;
  const char* mqttUsername() const;
  const char* mqttPassword() const;
  const char* mqttClientId() const;
  const char* mqttBaseTopic() const;
  const char* haDiscoveryPrefix() const;
  bool haDiscoveryEnabled() const;
  const char* yoRadioHost() const;
  uint16_t yoRadioPort() const;
  const char* yoRadioPath() const;
  const char* timezoneName() const;
  const char* timezoneRule() const;
  uint32_t uiPageTimeoutMs() const;

 private:
  struct Values : PublicValues {
    char wifiPassword[65]{};
    char mqttPassword[65]{};
  };

  static void setDefaults(Values& values);
  static bool validate(const Values& values);
  static bool validateAndRepair(Values& values, const Values& defaults);
  static uint32_t checksum(const Values& values);
  static void logInvalidField(const char* key);

  Values values_{};
  Source source_{Source::DEFAULTS};
};
