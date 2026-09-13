#pragma once

#include <Arduino.h>
#include <MQTT.h>
#include <WiFiClient.h>

#include "config.h"

struct AppState;
class RuntimeConfig;
enum class BinaryState : uint8_t;
enum class HvacMode : uint8_t;
enum class PresetMode : uint8_t;

// Centralne końcówki topiców danych i przyszłych komend Home Assistant.
namespace HomeAssistantTopicSuffix {
constexpr char STATE_WEATHER[] = "/ha/state/weather";
constexpr char STATE_CLIMATE[] = "/ha/state/climate";
constexpr char STATE_BOILER_POWER[] = "/ha/state/boiler_power";
constexpr char STATE_WINDOWS[] = "/ha/state/windows";
constexpr char STATE_CALENDAR[] = "/ha/state/calendar";
constexpr char STATE_WASTE[] = "/ha/state/waste";

constexpr char COMMAND_HVAC_MODE[] = "/ha/command/climate/hvac_mode";
constexpr char COMMAND_PRESET_MODE[] = "/ha/command/climate/preset_mode";
constexpr char COMMAND_TEMPERATURE[] = "/ha/command/climate/temperature";
constexpr char COMMAND_BOILER_POWER[] = "/ha/command/boiler_power";
}  // namespace HomeAssistantTopicSuffix

class HomeAssistantMqtt {
 public:
  HomeAssistantMqtt(AppState& state, const RuntimeConfig& config);
  void begin();
  void loop();
  void requestSnapshot();
  bool publishHvacMode(HvacMode mode);
  bool publishPresetMode(PresetMode mode);
  bool publishTargetTemperature(float temperature);
  bool publishBoilerPower(BinaryState state);

 private:
  enum class Phase : uint8_t {
    IDLE,
    WAIT_RETRY,
    CONNECTED,
  };

  struct Topics {
    char status[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char snapshotRequest[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char homeAssistantStatus[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char weather[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char climate[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char boilerPower[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char windows[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char calendar[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char waste[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char commandHvacMode[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char commandPresetMode[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char commandTemperature[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char commandBoilerPower[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char yoRadio[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char nextion[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char wifiRssi[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char uptime[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
    char firmware[Config::Mqtt::TOPIC_BUFFER_SIZE]{};
  };

  struct DiagnosticCache {
    bool yoRadioValid{false};
    bool yoRadioOnline{false};
    bool nextionValid{false};
    bool nextionOnline{false};
    bool wifiRssiValid{false};
    int32_t wifiRssi{0};
    bool uptimeValid{false};
    uint32_t uptimeSeconds{0};
    bool firmwareValid{false};
  };

  bool initializeIdentity();
  bool buildTopics();
  bool buildTopic(char* target, size_t targetSize, const char* suffix);
  void attemptConnection();
  void handleConnected();
  void handleDisconnected(bool scheduleReconnect);
  void handleWifiLost();
  void scheduleRetry();
  void advanceBackoff();
  void handleMessage(String& topic, String& payload);
  bool subscribeStateTopics();
  void parseWeather(const String& payload);
  void parseClimate(const String& payload);
  void parseBinaryState(const String& payload, bool boilerPower);
  void parseCalendar(const String& payload);
  void parseWaste(const String& payload);
  void startSnapshotSynchronization();
  void serviceSnapshotSynchronization(uint32_t now);
  uint8_t receivedGroupCount() const;
  void finishSnapshotIfComplete();
  void servicePendingPublications();
  bool publishMessage(const char* topic, const char* payload, bool retained,
                      int qos);
  bool publishCommand(const char* topic, const char* payload);
  bool publishDiscoveryEntity(uint8_t index);
  bool publishDiagnostic(uint8_t index);
  bool publishPeriodicDiagnostic(uint8_t index);
  bool publishYoRadioStatus();
  bool publishNextionStatus();
  bool publishWifiRssi();
  bool publishUptime();
  bool publishFirmware();
  void invalidatePublishCache();
  void startDiscoveryPublish();
  void startDiagnosticSnapshot();

  AppState& state_;
  const RuntimeConfig& config_;
  WiFiClient networkClient_;
  MQTTClient mqttClient_;
  Topics topics_;
  DiagnosticCache cache_;
  Phase phase_{Phase::IDLE};
  uint32_t observedWifiGeneration_{0};
  uint32_t retryStartedMs_{0};
  uint32_t retryDelayMs_{0};
  uint32_t lastPeriodicDiagnosticsMs_{0};
  uint32_t snapshotLastRequestMs_{0};
  uint8_t backoffStep_{0};
  uint8_t discoveryIndex_{0};
  uint8_t diagnosticIndex_{0};
  uint8_t periodicDiagnosticIndex_{0};
  bool configurationAvailable_{false};
  bool wifiWasConnected_{false};
  bool snapshotRequestPending_{false};
  bool snapshotSynchronizationActive_{false};
  bool discoveryPublishActive_{false};
  bool diagnosticSnapshotActive_{false};
  bool periodicDiagnosticsActive_{false};
  char deviceIdentifier_[48]{};
  char objectId_[48]{};
};
