#pragma once

#include <Arduino.h>

struct AppState;
class RuntimeConfig;

class YoRadioClient {
 public:
  YoRadioClient(AppState& state, const RuntimeConfig& config);
  void begin();
  void loop();

  bool previous();
  bool togglePlayPause();
  bool next();
  bool volumeDown();
  bool volumeUp();

 private:
  enum class Phase : uint8_t {
    IDLE,
    CONNECTING,
    WAIT_FIRST_DATA,
    ONLINE,
    WAIT_RETRY,
  };

  void startSession();
  void closeActiveSocket();
  void handleWifiLost();
  void scheduleRetry();
  void advanceBackoff();
  void clearSessionData();
  void clearMediaData();
  void handleEvent(uint8_t type, uint8_t* payload, size_t length,
                   uint32_t callbackSessionId, uint8_t callbackSlot);
  bool processMessage(const uint8_t* payload, size_t length, bool& changed);
  bool sendCommand(const char* command, bool clearMedia);
  void invalidateSession();

  AppState& state_;
  const RuntimeConfig& config_;
  Phase phase_{Phase::IDLE};
  uint32_t observedWifiGeneration_{0};
  uint32_t sessionId_{0};
  uint32_t sessionStartedMs_{0};
  uint32_t socketConnectedMs_{0};
  uint32_t retryStartedMs_{0};
  uint32_t retryDelayMs_{0};
  uint8_t backoffStep_{0};
  uint8_t activeSocketSlot_{0};
  uint8_t nextSocketSlot_{0};
  bool wifiWasConnected_{false};
  bool socketStarted_{false};
};
