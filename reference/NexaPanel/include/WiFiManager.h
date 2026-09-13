#pragma once

#include <Arduino.h>

struct AppState;
class RuntimeConfig;

class WiFiManager {
 public:
  WiFiManager(AppState& state, const RuntimeConfig& config);
  void begin();
  void loop();

 private:
  enum class Phase : uint8_t {
    IDLE,
    CONNECTING,
    CONNECTED,
    WAIT_RETRY,
  };

  bool configureNetwork();
  void startConnection();
  void handleConnected();
  void handleDisconnected();
  void scheduleRetry();
  void refreshConnectionData();
  void advanceBackoff();

  AppState& state_;
  const RuntimeConfig& config_;
  Phase phase_{Phase::IDLE};
  uint32_t connectStartedMs_{0};
  uint32_t retryStartedMs_{0};
  uint32_t retryDelayMs_{0};
  uint32_t currentBackoffMs_{0};
  uint32_t lastStateRefreshMs_{0};
  bool configurationAvailable_{false};
};
