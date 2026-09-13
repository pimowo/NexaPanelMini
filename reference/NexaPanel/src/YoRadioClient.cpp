#include "YoRadioClient.h"

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "AppState.h"
#include "RuntimeConfig.h"
#include "config.h"

namespace {

constexpr uint8_t SOCKET_SLOT_COUNT = 2;
WebSocketsClient sockets[SOCKET_SLOT_COUNT];

template <typename T>
bool setStateValue(StateValue<T>& target, const T& value) {
  const bool changed = !target.received || !target.valid || target.value != value;
  target.value = value;
  target.received = true;
  target.valid = true;
  return changed;
}

template <typename T>
void invalidateStateValue(StateValue<T>& target) {
  target.value = T{};
  target.received = false;
  target.valid = false;
}

template <typename T>
bool setInvalidReceivedState(StateValue<T>& target) {
  const bool changed = !target.received || target.valid || target.value != T{};
  target.value = T{};
  target.received = true;
  target.valid = false;
  return changed;
}

bool jsonValueToInt(JsonVariantConst value, int& result) {
  if (value.is<int>()) {
    result = value.as<int>();
    return true;
  }

  if (!value.is<const char*>()) {
    return false;
  }

  const char* text = value.as<const char*>();
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  result = static_cast<int>(parsed);
  return true;
}

bool isMetadataEdgeWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\r' ||
         character == '\n' || character == '\v' || character == '\f';
}

void trimMetadataBounds(const char*& begin, size_t& length) {
  while (length > 0 && isMetadataEdgeWhitespace(*begin)) {
    ++begin;
    --length;
  }
  while (length > 0 &&
         isMetadataEdgeWhitespace(begin[length - 1])) {
    --length;
  }
}

String trimmedMetadataSpan(const char* begin, size_t length) {
  trimMetadataBounds(begin, length);
  return String(begin, static_cast<unsigned int>(length));
}

RadioPlaybackState normalizePlayerState(const String& raw) {
  String normalized = raw;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "play" || normalized == "playing" ||
      normalized == "radio") {
    return RadioPlaybackState::PLAYING;
  }
  if (normalized == "stop" || normalized == "stopped") {
    return RadioPlaybackState::STOPPED;
  }
  if (normalized == "paused") {
    return RadioPlaybackState::PAUSED;
  }
  if (normalized == "connecting") {
    return RadioPlaybackState::CONNECTING;
  }
  if (normalized == "error") {
    return RadioPlaybackState::ERROR;
  }
  return RadioPlaybackState::UNKNOWN;
}

bool applyMeta(RadioState& radio, const char* meta) {
  const char* safeMeta = meta != nullptr ? meta : "";
  bool changed = setStateValue(radio.artist, String());

  if (strstr(safeMeta, "[connecting]") || strstr(safeMeta, "[łącze]") ||
      strstr(safeMeta, "[loading]") || strstr(safeMeta, "[buffering]")) {
    changed |= setStateValue(radio.title, String("Łączę"));
    changed |=
        setStateValue(radio.playback, RadioPlaybackState::CONNECTING);
    return changed;
  }
  if (strstr(safeMeta, "[stopped]") || strstr(safeMeta, "[zatrzymany]") ||
      strstr(safeMeta, "[stop]")) {
    changed |= setStateValue(radio.title, String("Zatrzymany"));
    changed |= setStateValue(radio.playback, RadioPlaybackState::STOPPED);
    return changed;
  }
  if (strstr(safeMeta, "[paused]") || strstr(safeMeta, "[pauza]")) {
    changed |= setStateValue(radio.title, String("Pauza"));
    changed |= setStateValue(radio.playback, RadioPlaybackState::PAUSED);
    return changed;
  }
  if (strstr(safeMeta, "[error]") || strstr(safeMeta, "[błąd]")) {
    changed |= setStateValue(radio.title, String("Błąd"));
    changed |= setStateValue(radio.playback, RadioPlaybackState::ERROR);
    return changed;
  }

  const size_t metaLength = strlen(safeMeta);
  const char* separator = strstr(safeMeta, " - ");
  if (separator != nullptr) {
    const size_t artistLength =
        static_cast<size_t>(separator - safeMeta);
    const char* titleBegin = separator + 3;
    const size_t titleLength =
        metaLength - static_cast<size_t>(titleBegin - safeMeta);
    changed |= setStateValue(
        radio.artist, trimmedMetadataSpan(safeMeta, artistLength));
    changed |= setStateValue(
        radio.title, trimmedMetadataSpan(titleBegin, titleLength));
    return changed;
  }

  const char* trimmedBegin = safeMeta;
  size_t trimmedLength = metaLength;
  trimMetadataBounds(trimmedBegin, trimmedLength);
  if (trimmedLength >= 2 && trimmedBegin[0] == '[' &&
      trimmedBegin[trimmedLength - 1] == ']') {
    ++trimmedBegin;
    trimmedLength -= 2;
    trimMetadataBounds(trimmedBegin, trimmedLength);
  }
  changed |= setStateValue(
      radio.title,
      String(trimmedBegin, static_cast<unsigned int>(trimmedLength)));
  return changed;
}

}  // namespace

YoRadioClient::YoRadioClient(AppState& state, const RuntimeConfig& config)
    : state_(state), config_(config) {}

void YoRadioClient::begin() {
  state_.radio.connection = RadioConnectionState::DISCONNECTED;
  state_.radio.firstDataReceived = false;
  clearSessionData();
}

void YoRadioClient::loop() {
  const uint32_t now = millis();

  if (!state_.network.connected) {
    if (wifiWasConnected_ || socketStarted_ || phase_ != Phase::IDLE) {
      handleWifiLost();
    }
    return;
  }

  if (!wifiWasConnected_ ||
      observedWifiGeneration_ != state_.network.sessionGeneration) {
    wifiWasConnected_ = true;
    observedWifiGeneration_ = state_.network.sessionGeneration;
    backoffStep_ = 0;
    closeActiveSocket();
    startSession();
    return;
  }

  if (socketStarted_) {
    sockets[activeSocketSlot_].loop();
  }

  if (phase_ == Phase::CONNECTING &&
      static_cast<uint32_t>(now - sessionStartedMs_) >=
          Config::YoRadio::CONNECT_TIMEOUT_MS) {
    closeActiveSocket();
    state_.radio.connection = RadioConnectionState::OFFLINE;
    state_.diagnostics.yoRadio.lastDisconnectReason =
        YoRadioDisconnectReason::CONNECT_TIMEOUT;
    Serial.println("YORADIO DISCONNECTED");
    scheduleRetry();
    return;
  }

  if (phase_ == Phase::WAIT_FIRST_DATA &&
      static_cast<uint32_t>(now - socketConnectedMs_) >=
          Config::YoRadio::FIRST_DATA_TIMEOUT_MS) {
    closeActiveSocket();
    state_.radio.connection = RadioConnectionState::OFFLINE;
    state_.diagnostics.yoRadio.lastDisconnectReason =
        YoRadioDisconnectReason::FIRST_DATA_TIMEOUT;
    Serial.println("YORADIO DISCONNECTED");
    scheduleRetry();
    return;
  }

  if (phase_ == Phase::WAIT_RETRY &&
      static_cast<uint32_t>(now - retryStartedMs_) >= retryDelayMs_) {
    startSession();
  }
}

bool YoRadioClient::previous() { return sendCommand("prev=1", true); }

bool YoRadioClient::togglePlayPause() {
  return sendCommand("toggle=1", false);
}

bool YoRadioClient::next() { return sendCommand("next=1", true); }

bool YoRadioClient::volumeDown() { return sendCommand("volm=1", false); }

bool YoRadioClient::volumeUp() { return sendCommand("volp=1", false); }

void YoRadioClient::startSession() {
  ++state_.diagnostics.yoRadio.connectionAttempts;
  activeSocketSlot_ = nextSocketSlot_;
  nextSocketSlot_ =
      static_cast<uint8_t>((nextSocketSlot_ + 1U) % SOCKET_SLOT_COUNT);
  invalidateSession();

  const uint32_t callbackSessionId = sessionId_;
  const uint8_t callbackSlot = activeSocketSlot_;
  WebSocketsClient& socket = sockets[activeSocketSlot_];
  socket.disconnect();
  socket.onEvent([this, callbackSessionId, callbackSlot](
                     WStype_t type, uint8_t* payload, size_t length) {
    handleEvent(static_cast<uint8_t>(type), payload, length,
                callbackSessionId, callbackSlot);
  });

  clearSessionData();
  state_.radio.connection = RadioConnectionState::CONNECTING;
  state_.radio.firstDataReceived = false;
  phase_ = Phase::CONNECTING;
  sessionStartedMs_ = millis();
  socketStarted_ = true;

  Serial.println("YORADIO CONNECTING");
  socket.begin(config_.yoRadioHost(), config_.yoRadioPort(),
               config_.yoRadioPath());
  // Retry biblioteki nie zdąży wejść przed naszym kontrolowanym timeoutem.
  socket.setReconnectInterval(Config::YoRadio::CONNECT_TIMEOUT_MS + 1000U);
}

void YoRadioClient::closeActiveSocket() {
  invalidateSession();
  if (!socketStarted_) {
    return;
  }

  socketStarted_ = false;
  sockets[activeSocketSlot_].disconnect();
}

void YoRadioClient::handleWifiLost() {
  const bool hadSession =
      socketStarted_ || phase_ == Phase::ONLINE ||
      phase_ == Phase::WAIT_FIRST_DATA || phase_ == Phase::CONNECTING;
  closeActiveSocket();
  wifiWasConnected_ = false;
  phase_ = Phase::IDLE;
  state_.radio.connection = RadioConnectionState::OFFLINE;
  state_.radio.firstDataReceived = false;
  clearSessionData();
  if (hadSession) {
    ++state_.diagnostics.yoRadio.disconnects;
    state_.diagnostics.yoRadio.lastDisconnectReason =
        YoRadioDisconnectReason::WIFI_LOST;
    Serial.println("YORADIO DISCONNECTED");
  }
}

void YoRadioClient::scheduleRetry() {
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
    retryDelayMs_ = Config::YoRadio::RECONNECT_MAX_MS;
    state_.diagnostics.yoRadio.currentBackoffMs = retryDelayMs_;
    Serial.printf("YORADIO RETRY in %lu ms\n",
                  static_cast<unsigned long>(retryDelayMs_));
    return;
  }

  const uint64_t candidate =
      static_cast<uint64_t>(Config::YoRadio::RECONNECT_MIN_MS) * multiplier;
  retryDelayMs_ = candidate > Config::YoRadio::RECONNECT_MAX_MS
                      ? Config::YoRadio::RECONNECT_MAX_MS
                      : static_cast<uint32_t>(candidate);
  state_.diagnostics.yoRadio.currentBackoffMs = retryDelayMs_;
  Serial.printf("YORADIO RETRY in %lu ms\n",
                static_cast<unsigned long>(retryDelayMs_));
  advanceBackoff();
}

void YoRadioClient::advanceBackoff() {
  if (backoffStep_ < 4U) {
    ++backoffStep_;
  }
}

void YoRadioClient::clearSessionData() {
  clearMediaData();
  invalidateStateValue(state_.radio.volume);
  invalidateStateValue(state_.radio.bitrate);
  invalidateStateValue(state_.radio.format);
  invalidateStateValue(state_.radio.rawPlayerState);
  invalidateStateValue(state_.radio.playback);
  invalidateStateValue(state_.radio.rssi);
  invalidateStateValue(state_.radio.heap);
  invalidateStateValue(state_.radio.bass);
  invalidateStateValue(state_.radio.middle);
  invalidateStateValue(state_.radio.trebble);
  invalidateStateValue(state_.radio.balance);
}

void YoRadioClient::clearMediaData() {
  invalidateStateValue(state_.radio.station);
  invalidateStateValue(state_.radio.artist);
  invalidateStateValue(state_.radio.title);
}

void YoRadioClient::handleEvent(uint8_t type, uint8_t* payload, size_t length,
                                uint32_t callbackSessionId,
                                uint8_t callbackSlot) {
  if (!socketStarted_ || !state_.network.connected ||
      callbackSessionId != sessionId_ ||
      callbackSlot != activeSocketSlot_) {
    return;
  }

  switch (static_cast<WStype_t>(type)) {
    case WStype_CONNECTED:
      if (state_.diagnostics.yoRadio.successfulConnections > 0) {
        ++state_.diagnostics.yoRadio.reconnects;
      }
      ++state_.diagnostics.yoRadio.successfulConnections;
      state_.diagnostics.yoRadio.lastSuccessfulConnectionMs = millis();
      state_.diagnostics.yoRadio.currentBackoffMs = 0;
      phase_ = Phase::WAIT_FIRST_DATA;
      socketConnectedMs_ = millis();
      Serial.println("YORADIO CONNECTED");
      sockets[activeSocketSlot_].sendTXT("getindex=1");
      Serial.println("YORADIO GETINDEX");
      break;

    case WStype_DISCONNECTED:
      socketStarted_ = false;
      state_.radio.connection = RadioConnectionState::OFFLINE;
      state_.radio.firstDataReceived = false;
      ++state_.diagnostics.yoRadio.disconnects;
      state_.diagnostics.yoRadio.lastDisconnectReason =
          YoRadioDisconnectReason::REMOTE_DISCONNECT;
      Serial.println("YORADIO DISCONNECTED");
      scheduleRetry();
      break;

    case WStype_TEXT: {
      if (phase_ != Phase::WAIT_FIRST_DATA && phase_ != Phase::ONLINE) {
        break;
      }

      if (Config::Logging::DEBUG_WEBSOCKET_RAW) {
        Serial.printf("YORADIO RAW RX (%u): ",
                      static_cast<unsigned>(length));
        Serial.write(payload, length);
        Serial.println();
      }

      bool changed = false;
      if (!processMessage(payload, length, changed)) {
        break;
      }

      state_.diagnostics.yoRadio.lastDataMs = millis();

      if (!state_.radio.firstDataReceived) {
        state_.radio.firstDataReceived = true;
        state_.radio.connection = RadioConnectionState::ONLINE;
        phase_ = Phase::ONLINE;
        backoffStep_ = 0;
        ++state_.diagnostics.yoRadio.firstDataCount;
        Serial.println("YORADIO FIRST DATA");
      }

      if (changed && Config::Logging::DEBUG_WEBSOCKET) {
        Serial.println("YORADIO STATE changed");
      }
      break;
    }

    case WStype_ERROR:
      socketStarted_ = false;
      state_.radio.connection = RadioConnectionState::ERROR;
      state_.radio.firstDataReceived = false;
      ++state_.diagnostics.yoRadio.disconnects;
      state_.diagnostics.yoRadio.lastDisconnectReason =
          YoRadioDisconnectReason::WEBSOCKET_ERROR;
      Serial.println("YORADIO DISCONNECTED");
      scheduleRetry();
      break;

    default:
      break;
  }
}

bool YoRadioClient::processMessage(const uint8_t* payload, size_t length,
                                   bool& changed) {
  StaticJsonDocument<1024> document;
  const DeserializationError error =
      deserializeJson(document, payload, length);
  if (error) {
    if (Config::Logging::DEBUG_WEBSOCKET) {
      Serial.printf("YORADIO JSON ERROR: %s\n", error.c_str());
    }
    return false;
  }

  JsonArrayConst entries = document["payload"].as<JsonArrayConst>();
  if (entries.isNull()) {
    return false;
  }

  bool recognized = false;
  for (JsonObjectConst entry : entries) {
    const char* id = entry["id"] | "";
    JsonVariantConst value = entry["value"];

    if (strcmp(id, "nameset") == 0 && value.is<const char*>()) {
      changed |= setStateValue(state_.radio.station,
                               String(value.as<const char*>()));
      recognized = true;
    } else if (strcmp(id, "meta") == 0 && value.is<const char*>()) {
      changed |= applyMeta(state_.radio, value.as<const char*>());
      recognized = true;
    } else if (strcmp(id, "fmt") == 0 &&
               value.is<const char*>()) {
      const String format = value.as<const char*>();
      changed |= setStateValue(
          state_.radio.format,
          format == "bitrate" ? String("kbs") : format);
      recognized = true;
    } else if (strcmp(id, "playerwrap") == 0 &&
               value.is<const char*>()) {
      const String raw = value.as<const char*>();
      changed |= setStateValue(state_.radio.rawPlayerState, raw);
      changed |=
          setStateValue(state_.radio.playback, normalizePlayerState(raw));
      recognized = true;
    } else {
      int number = 0;
      if (strcmp(id, "volume") == 0) {
        if (jsonValueToInt(value, number)) {
          if (number >= Config::YoRadio::VOLUME_MIN &&
              number <= Config::YoRadio::VOLUME_MAX) {
            changed |= setStateValue(state_.radio.volume, number);
          } else {
            changed |= setInvalidReceivedState(state_.radio.volume);
          }
          recognized = true;
        }
      } else if (strcmp(id, "bitrate") == 0) {
        if (jsonValueToInt(value, number)) {
          if (number >= Config::YoRadio::BITRATE_MIN &&
              number <= Config::YoRadio::BITRATE_MAX) {
            changed |= setStateValue(state_.radio.bitrate, number);
          } else {
            changed |= setInvalidReceivedState(state_.radio.bitrate);
          }
          recognized = true;
        }
      } else if (strcmp(id, "rssi") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.rssi, number);
          recognized = true;
        }
      } else if (strcmp(id, "heap") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.heap, number);
          recognized = true;
        }
      } else if (strcmp(id, "bass") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.bass, number);
          recognized = true;
        }
      } else if (strcmp(id, "middle") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.middle, number);
          recognized = true;
        }
      } else if (strcmp(id, "trebble") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.trebble, number);
          recognized = true;
        }
      } else if (strcmp(id, "balance") == 0) {
        if (jsonValueToInt(value, number)) {
          changed |= setStateValue(state_.radio.balance, number);
          recognized = true;
        }
      } else if (Config::Logging::DEBUG_WEBSOCKET) {
        Serial.printf("YORADIO UNKNOWN FIELD: %s\n",
                      id[0] != '\0' ? id : "<missing>");
      }
    }
  }

  return recognized;
}

bool YoRadioClient::sendCommand(const char* command, bool clearMedia) {
  if (command == nullptr || command[0] == '\0' ||
      phase_ != Phase::ONLINE || !socketStarted_ ||
      !state_.radio.firstDataReceived ||
      !sockets[activeSocketSlot_].isConnected()) {
    return false;
  }

  const bool sent = sockets[activeSocketSlot_].sendTXT(command);
  if (!sent) {
    return false;
  }

  if (clearMedia) {
    clearMediaData();
  }
  if (Config::Logging::DEBUG_WEBSOCKET_RAW) {
    Serial.printf("YORADIO RAW TX: %s\n", command);
  }
  return true;
}

void YoRadioClient::invalidateSession() {
  ++sessionId_;
  if (sessionId_ == 0U) {
    ++sessionId_;
  }
}
