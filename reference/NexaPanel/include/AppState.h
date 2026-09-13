#pragma once

#include <Arduino.h>
#include <time.h>

#include "config.h"

// Wartość pochodząca ze źródła zewnętrznego wraz z informacją, czy została już
// odebrana i czy może być obecnie używana.
template <typename T>
struct StateValue {
  T value{};
  bool received{false};
  bool valid{false};
};

// Tekst UTF-8 o stałej maksymalnej pojemności wraz ze stanem wartości.
template <size_t N>
struct StateText {
  char value[N + 1]{};
  bool received{false};
  bool valid{false};
};

enum class RadioConnectionState : uint8_t {
  DISCONNECTED,
  CONNECTING,
  ONLINE,
  OFFLINE,
  ERROR,
};

enum class RadioPlaybackState : uint8_t {
  UNKNOWN,
  PLAYING,
  PAUSED,
  STOPPED,
  CONNECTING,
  ERROR,
};

enum class HvacMode : uint8_t {
  UNKNOWN,
  OFF,
  HEAT,
};

enum class PresetMode : uint8_t {
  UNKNOWN,
  NONE,
  COMFORT,
  SLEEP,
};

enum class HvacAction : uint8_t {
  UNKNOWN,
  IDLE,
  HEATING,
  OFF,
};

enum class BinaryState : uint8_t {
  UNKNOWN,
  OFF,
  ON,
};

enum class UiPage : uint8_t {
  UNKNOWN,
  BOOT,
  START,
  RADIO,
  BOILER,
  UPDATE,
};

struct RadioState {
  RadioConnectionState connection{RadioConnectionState::DISCONNECTED};
  bool firstDataReceived{false};
  StateValue<String> station;
  StateValue<String> artist;
  StateValue<String> title;
  StateValue<int> volume;
  StateValue<int> bitrate;
  StateValue<String> format;
  StateValue<String> rawPlayerState;
  StateValue<RadioPlaybackState> playback;
  StateValue<int> rssi;
  StateValue<int> heap;
  StateValue<int> bass;
  StateValue<int> middle;
  StateValue<int> trebble;
  StateValue<int> balance;
};

struct HomeAssistantGroupState {
  bool received{false};
  bool valid{false};
  bool parseError{false};
  uint32_t lastUpdateMs{0};
};

struct HomeAssistantWeatherState {
  HomeAssistantGroupState group;
  StateValue<float> temperature;
};

struct HomeAssistantClimateState {
  HomeAssistantGroupState group;
  StateValue<HvacMode> hvacMode;
  StateText<Config::HomeAssistant::CLIMATE_RAW_TEXT_MAX_BYTES> hvacModeRaw;
  StateValue<PresetMode> presetMode;
  StateText<Config::HomeAssistant::CLIMATE_RAW_TEXT_MAX_BYTES> presetModeRaw;
  StateValue<HvacAction> hvacAction;
  StateText<Config::HomeAssistant::CLIMATE_RAW_TEXT_MAX_BYTES> hvacActionRaw;
  StateValue<float> targetTemperature;
  StateValue<float> currentTemperature;
};

struct HomeAssistantBinaryState {
  HomeAssistantGroupState group;
  StateValue<BinaryState> state;
};

struct HomeAssistantCalendarState {
  HomeAssistantGroupState group;
  StateText<Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES> today[4];
  StateText<Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES> tomorrow[4];
};

struct HomeAssistantWasteState {
  HomeAssistantGroupState group;
  StateText<Config::HomeAssistant::WASTE_TEXT_MAX_BYTES> mixed;
  StateText<Config::HomeAssistant::WASTE_TEXT_MAX_BYTES> paper;
  StateText<Config::HomeAssistant::WASTE_TEXT_MAX_BYTES> glass;
  StateText<Config::HomeAssistant::WASTE_TEXT_MAX_BYTES> plastic;
};

struct HomeAssistantState {
  bool mqttConnected{false};
  bool mqttConnecting{false};
  uint32_t mqttSessionGeneration{0};
  uint32_t mqttLastConnectedMs{0};
  uint32_t mqttLastDisconnectedMs{0};
  bool homeAssistantOnline{false};
  bool snapshotComplete{false};
  uint8_t snapshotReceivedGroups{0};
  uint32_t snapshotLastCompleteMs{0};
  HomeAssistantWeatherState weather;
  HomeAssistantClimateState climate;
  HomeAssistantBinaryState boilerPower;
  HomeAssistantBinaryState windows;
  HomeAssistantCalendarState calendar;
  HomeAssistantWasteState waste;
};

struct NetworkState {
  bool connected{false};
  bool connecting{false};
  String ipAddress;
  int32_t rssi{0};
  uint32_t sessionGeneration{0};
  uint32_t sessionStartedMs{0};
  uint32_t lastDisconnectedMs{0};
};

struct ClockState {
  bool timeValid{false};
  time_t lastSyncEpoch{0};
  time_t lastValidEpoch{0};
  uint32_t syncGeneration{0};
};

struct UiState {
  UiPage currentPage{UiPage::UNKNOWN};
  bool nextionReady{false};
  bool nextionSynchronized{false};
  bool fullRefreshRequested{true};
  bool activePageCacheValid{false};
  uint32_t lastTouchMs{0};
  bool optimisticTargetActive{false};
  float optimisticTargetValue{0.0F};
  uint32_t optimisticTargetStartedMs{0};
};

enum class YoRadioDisconnectReason : uint8_t {
  NONE,
  WIFI_LOST,
  CONNECT_TIMEOUT,
  FIRST_DATA_TIMEOUT,
  REMOTE_DISCONNECT,
  WEBSOCKET_ERROR,
};

struct WifiDiagnostics {
  uint32_t connectionAttempts{0};
  uint32_t successfulConnections{0};
  uint32_t disconnects{0};
  uint32_t connectTimeouts{0};
  uint32_t reconnects{0};
  uint32_t lastConnectedMs{0};
  uint32_t lastDisconnectedMs{0};
  int32_t minimumRssi{0};
  bool minimumRssiValid{false};
};

struct MqttDiagnostics {
  uint32_t connectionAttempts{0};
  uint32_t successfulConnections{0};
  uint32_t disconnects{0};
  uint32_t reconnects{0};
  uint32_t lastSuccessfulConnectionMs{0};
  uint32_t lastDisconnectedMs{0};
  uint32_t lastHaStateMs{0};
  uint32_t lastSnapshotCompleteMs{0};
  uint32_t discoveryPublishCount{0};
  int32_t lastError{0};
  int32_t lastReturnCode{0};
};

struct YoRadioDiagnostics {
  uint32_t connectionAttempts{0};
  uint32_t successfulConnections{0};
  uint32_t disconnects{0};
  uint32_t reconnects{0};
  uint32_t firstDataCount{0};
  uint32_t lastDataMs{0};
  uint32_t lastSuccessfulConnectionMs{0};
  uint32_t currentBackoffMs{0};
  YoRadioDisconnectReason lastDisconnectReason{YoRadioDisconnectReason::NONE};
};

struct NextionDiagnostics {
  uint32_t receivedFrames{0};
  uint32_t touchFrames{0};
  uint32_t pageFrames{0};
  uint32_t error1AFrames{0};
  uint32_t parserTimeouts{0};
  uint32_t malformedFrames{0};
  uint32_t txQueued{0};
  uint32_t txSent{0};
  uint32_t txQueueHighWater{0};
  uint32_t txDropped{0};
  uint32_t startupSyncRetries{0};
  uint32_t pageVerifyRetries{0};
  uint32_t tftUpdateSuccesses{0};
  uint32_t tftUpdateFailures{0};
};

struct LoopDiagnostics {
  uint64_t iterations{0};
  uint64_t totalDurationUs{0};
  uint32_t rollingAverageUs{0};
  uint32_t maximumDurationUs{0};
  uint32_t over10Ms{0};
  uint32_t over50Ms{0};
  uint32_t over100Ms{0};
};

struct HttpDiagnostics {
  uint32_t requests{0};
  uint32_t responses2xx{0};
  uint32_t responses4xx{0};
  uint32_t responses5xx{0};
  uint32_t firmwareUpdateSuccesses{0};
  uint32_t firmwareUpdateFailures{0};
  uint32_t configSaveSuccesses{0};
  uint32_t configSaveFailures{0};
  uint32_t configImportSuccesses{0};
  uint32_t configImportFailures{0};
};

struct SystemDiagnostics {
  uint32_t resetReasonCode{0};
  char resetReason[16]{"UNKNOWN"};
  uint32_t loopStackHighWaterBytes{0};
  uint32_t minimumLoopStackHighWaterBytes{0};
  bool lowStackWarningLogged{false};
};

struct DiagnosticsState {
  WifiDiagnostics wifi;
  MqttDiagnostics mqtt;
  YoRadioDiagnostics yoRadio;
  NextionDiagnostics nextion;
  LoopDiagnostics loop;
  HttpDiagnostics http;
  SystemDiagnostics system;
};

struct AppState {
  RadioState radio;
  HomeAssistantState homeAssistant;
  NetworkState network;
  ClockState clock;
  UiState ui;
  DiagnosticsState diagnostics;
};
