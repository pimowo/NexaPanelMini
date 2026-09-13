#pragma once

#include <Arduino.h>
#include <time.h>

struct AppState;
class RuntimeConfig;

class ClockService {
 public:
  ClockService(AppState& state, const RuntimeConfig& config);
  void begin();
  void loop();

  bool formatTime(char* buffer, size_t bufferSize) const;
  bool formatDate(char* buffer, size_t bufferSize) const;

 private:
  void startSynchronization();
  void processSynchronization();
  void refreshLastValidTime();
  bool readValidLocalTime(time_t& epoch, tm& localTime) const;
  void logLocalTime(const tm& localTime) const;

  AppState& state_;
  const RuntimeConfig& config_;
  uint32_t observedWifiGeneration_{0};
  uint32_t lastValidityCheckMs_{0};
  bool ntpStarted_{false};
  bool synchronizationReported_{false};
};
