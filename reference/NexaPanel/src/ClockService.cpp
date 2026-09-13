#include "ClockService.h"

#include <esp_sntp.h>
#include <stdlib.h>
#include <time.h>

#include "AppState.h"
#include "RuntimeConfig.h"
#include "config.h"

namespace {

volatile bool ntpSynchronizationPending = false;

void onNtpSynchronization(timeval*) {
  ntpSynchronizationPending = true;
}

}  // namespace

ClockService::ClockService(AppState& state, const RuntimeConfig& config)
    : state_(state), config_(config) {}

void ClockService::begin() {
  state_.clock.timeValid = false;
  state_.clock.lastSyncEpoch = 0;
  state_.clock.lastValidEpoch = 0;
  state_.clock.syncGeneration = 0;

  setenv("TZ", config_.timezoneRule(), 1);
  tzset();
  esp_sntp_set_time_sync_notification_cb(onNtpSynchronization);
  esp_sntp_set_sync_interval(Config::Ntp::SYNC_INTERVAL_MS);
}

void ClockService::loop() {
  if (state_.network.connected &&
      state_.network.sessionGeneration != observedWifiGeneration_) {
    observedWifiGeneration_ = state_.network.sessionGeneration;
    startSynchronization();
  }

  if (ntpSynchronizationPending) {
    ntpSynchronizationPending = false;
    processSynchronization();
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastValidityCheckMs_) >=
      Config::Ntp::VALIDITY_CHECK_INTERVAL_MS) {
    lastValidityCheckMs_ = now;
    refreshLastValidTime();
  }
}

bool ClockService::formatTime(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) {
    return false;
  }
  buffer[0] = '\0';

  time_t epoch;
  tm localTime{};
  if (!state_.clock.timeValid || !readValidLocalTime(epoch, localTime)) {
    return false;
  }
  return strftime(buffer, bufferSize, "%H:%M", &localTime) > 0;
}

bool ClockService::formatDate(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) {
    return false;
  }
  buffer[0] = '\0';

  time_t epoch;
  tm localTime{};
  if (!state_.clock.timeValid || !readValidLocalTime(epoch, localTime)) {
    return false;
  }
  return strftime(buffer, bufferSize, "%d.%m.%Y", &localTime) > 0;
}

void ClockService::startSynchronization() {
  ntpSynchronizationPending = false;
  esp_sntp_set_time_sync_notification_cb(onNtpSynchronization);
  esp_sntp_set_sync_interval(Config::Ntp::SYNC_INTERVAL_MS);

  if (!ntpStarted_) {
    Serial.println("NTP START");
    ntpStarted_ = true;
  }

  configTzTime(config_.timezoneRule(), Config::Ntp::SERVER_PRIMARY,
               Config::Ntp::SERVER_SECONDARY, Config::Ntp::SERVER_TERTIARY);
}

void ClockService::processSynchronization() {
  time_t epoch;
  tm localTime{};
  if (!readValidLocalTime(epoch, localTime)) {
    Serial.println("NTP WARNING: odebrany czas nie przeszedł walidacji");
    return;
  }

  if (state_.clock.syncGeneration > 0 &&
      state_.clock.lastSyncEpoch == epoch) {
    return;
  }

  state_.clock.timeValid = true;
  state_.clock.lastSyncEpoch = epoch;
  state_.clock.lastValidEpoch = epoch;
  ++state_.clock.syncGeneration;

  if (!synchronizationReported_) {
    Serial.println("NTP SYNCED");
    synchronizationReported_ = true;
  } else {
    Serial.println("NTP RESYNC");
  }
  logLocalTime(localTime);
}

void ClockService::refreshLastValidTime() {
  if (!state_.clock.timeValid) {
    return;
  }

  time_t epoch;
  tm localTime{};
  if (readValidLocalTime(epoch, localTime)) {
    state_.clock.lastValidEpoch = epoch;
  }
}

bool ClockService::readValidLocalTime(time_t& epoch, tm& localTime) const {
  time(&epoch);
  if (localtime_r(&epoch, &localTime) == nullptr) {
    return false;
  }
  return localTime.tm_year + 1900 >= Config::Ntp::MIN_VALID_YEAR;
}

void ClockService::logLocalTime(const tm& localTime) const {
  char date[16]{};
  char clock[8]{};
  char zone[16]{};
  strftime(date, sizeof(date), "%d.%m.%Y", &localTime);
  strftime(clock, sizeof(clock), "%H:%M", &localTime);
  strftime(zone, sizeof(zone), "%Z", &localTime);

  Serial.printf("NTP date: %s\n", date);
  Serial.printf("NTP time: %s\n", clock);
  Serial.printf("NTP timezone: %s (%s, DST=%s)\n", config_.timezoneName(),
                zone, localTime.tm_isdst > 0 ? "tak" : "nie");
}
