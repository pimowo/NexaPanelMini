#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

struct AppState;
class NextionDriver;
class NextionUi;
class RuntimeConfig;

class NextionUpdater {
 public:
  NextionUpdater(AppState& state, NextionDriver& driver, NextionUi& ui,
                 RuntimeConfig& config);

  void begin();
  void loop();
  bool active() const;

 private:
  enum class Phase : uint8_t {
    IDLE,
    READ_HTTP_HEADER,
    READ_JSON_BODY,
    RECEIVE_FIRMWARE,
    PREPARE_UPDATE_SCREEN,
    DRAIN_UPDATE_SCREEN_RX,
    WAIT_CONNECT_RESPONSE,
    WAIT_BAUD_SWITCH,
    WAIT_INITIAL_ACK,
    RECEIVE_BLOCK,
    WAIT_BLOCK_ACK,
    WAIT_NEXTION_REBOOT,
    WAIT_NORMAL_BAUD_SETTLE,
    WAIT_RECOVERY_RESPONSE,
    PREPARE_SUCCESS_SCREEN,
    PREPARE_ERROR_SCREEN,
    WAIT_ERROR_SCREEN_STABLE,
    WAIT_ESP_RESTART,
  };

  enum class JsonRequest : uint8_t {
    CONFIG_UPDATE,
    CONFIG_IMPORT,
  };

  void acceptClient();
  void readHttpHeader();
  void readJsonBody();
  void releaseJsonBody();
  void handleHttpRequest();
  void serveMainPage();
  void serveConfig();
  void exportConfig();
  void serveStatus();
  void applyJsonConfig();
  void importJsonConfig();
  void beginJsonRequest(size_t contentLength, JsonRequest request);
  void resetConfig();
  void scheduleRestart();
  void startFirmwareUpload(const char* fileName, size_t fileSize);
  void receiveFirmware();
  void failFirmwareUpload(int status, const char* reason,
                          const char* message);
  void serveUploadPage();
  void startUpload(const char* fileName, size_t fileSize);
  void prepareUpdateScreen();
  void drainUpdateScreenRx();
  void beginUploadProtocol();
  void waitForConnectResponse();
  void sendUploadCommand();
  void waitForInitialAck();
  void receiveBlock();
  void sendBufferedBlock();
  void waitForBlockAck();
  void beginNormalRecovery(bool afterSuccessfulUpload);
  void waitForNormalBaudSettle();
  void waitForRecoveryResponse();
  void completeNormalRecovery();
  void completeSuccessfulUpload();
  void failUpload(const char* reason);
  void failBeforeUpdateMode(const char* reason);
  void completeFailedRecovery();
  void prepareSuccessScreen();
  void prepareErrorScreen();
  void logProgress();
  bool readAck();
  bool readReady();
  bool readCompleteConnectResponse(bool& complete);
  bool readResponseToken(const char* token);
  bool requestTimedOut(uint32_t timeoutMs) const;
  void resetResponseBuffer();
  void recordHttpResponse(int status);
  void recordTftFailure();
  void closeHttpClient();
  void sendTextResponse(NetworkClient& client, int status,
                        const char* reason, const char* body);
  void sendJsonResponse(NetworkClient& client, int status,
                        const char* reason, const char* body);
  void sendBusyResponse(NetworkClient& client);

  AppState& state_;
  NextionDriver& driver_;
  NextionUi& ui_;
  RuntimeConfig& config_;
  NetworkServer server_;
  NetworkClient client_;
  Phase phase_{Phase::IDLE};
  JsonRequest jsonRequest_{JsonRequest::CONFIG_UPDATE};
  bool active_{false};
  bool firmwareUpdateActive_{false};
  uint8_t firmwareUpdateProgress_{0};
  bool recoveryAfterSuccessfulUpload_{false};
  bool resultScreenQueued_{false};
  bool tftFailureRecorded_{false};
  uint32_t phaseStartedMs_{0};
  uint32_t lastHttpActivityMs_{0};
  size_t headerLength_{0};
  char header_[Config::NextionUpdate::HTTP_HEADER_MAX_SIZE + 1]{};
  size_t expectedJsonBodyLength_{0};
  size_t jsonBodyLength_{0};
  char* jsonBody_{nullptr};
  char fileName_[Config::NextionUpdate::FILE_NAME_MAX_SIZE + 1]{};
  size_t totalBytes_{0};
  size_t receivedBytes_{0};
  size_t sentBytes_{0};
  size_t acknowledgedBytes_{0};
  size_t chunkLength_{0};
  size_t pendingBlockLength_{0};
  uint8_t nextProgressPercent_{10};
  uint8_t chunk_[Config::NextionUpdate::CHUNK_SIZE]{};
  char responseBuffer_[160]{};
  size_t responseLength_{0};
  uint8_t responseTerminatorCount_{0};
  size_t detectedFlashBytes_{0};
  char detectedModel_[40]{};
  char lastResult_[192]{"Brak wykonanej aktualizacji."};
  bool restartPending_{false};
  uint32_t restartScheduledMs_{0};
  uint32_t restartDelayMs_{Config::NextionUpdate::API_RESTART_DELAY_MS};
};
