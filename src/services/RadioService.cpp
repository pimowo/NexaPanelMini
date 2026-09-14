#include "services/RadioService.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "config.h"

namespace {
bool setString(String& target, const String& value) {
    if (target == value) return false;
    target = value;
    return true;
}

bool readInt(JsonVariantConst value, int& result) {
    if (value.is<int>()) {
        result = value.as<int>();
        return true;
    }
    if (!value.is<const char*>()) return false;
    const char* text = value.as<const char*>();
    if (!text || !text[0]) return false;
    char* end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0') return false;
    result = static_cast<int>(parsed);
    return true;
}

bool applyMeta(AppState& state, const char* value) {
    String meta = value ? value : "";
    meta.trim();
    String normalized = meta;
    normalized.toLowerCase();
    bool changed = false;

    if (normalized.indexOf("[connecting]") >= 0 ||
        normalized.indexOf("[łącze]") >= 0 ||
        normalized.indexOf("[loading]") >= 0 ||
        normalized.indexOf("[buffering]") >= 0) {
        changed |= setString(state.radioArtist, "");
        changed |= setString(state.radioTitle, "Łączę");
        if (state.radioPlaying) {
            state.radioPlaying = false;
            changed = true;
        }
        return changed;
    }
    if (normalized.indexOf("[stopped]") >= 0 ||
        normalized.indexOf("[zatrzymany]") >= 0 ||
        normalized.indexOf("[stop]") >= 0) {
        changed |= setString(state.radioArtist, "");
        changed |= setString(state.radioTitle, "Zatrzymany");
        if (state.radioPlaying) {
            state.radioPlaying = false;
            changed = true;
        }
        return changed;
    }
    if (normalized.indexOf("[paused]") >= 0 ||
        normalized.indexOf("[pauza]") >= 0) {
        changed |= setString(state.radioArtist, "");
        changed |= setString(state.radioTitle, "Pauza");
        if (state.radioPlaying) {
            state.radioPlaying = false;
            changed = true;
        }
        return changed;
    }
    if (normalized.indexOf("[error]") >= 0 ||
        normalized.indexOf("[błąd]") >= 0) {
        changed |= setString(state.radioArtist, "");
        changed |= setString(state.radioTitle, "Błąd");
        if (state.radioPlaying) {
            state.radioPlaying = false;
            changed = true;
        }
        return changed;
    }

    const int separator = meta.indexOf(" - ");
    if (separator >= 0) {
        String artist = meta.substring(0, separator);
        String title = meta.substring(separator + 3);
        artist.trim();
        title.trim();
        changed |= setString(state.radioArtist, artist);
        changed |= setString(state.radioTitle, title);
    } else {
        if (meta.length() >= 2 && meta[0] == '[' &&
            meta[meta.length() - 1] == ']') {
            meta = meta.substring(1, meta.length() - 1);
            meta.trim();
        }
        changed |= setString(state.radioArtist, "");
        changed |= setString(state.radioTitle, meta);
    }
    return changed;
}
}

void RadioService::begin(AppState& state) {
    state_ = &state;
    state.radioOfflineError = false;
    offlineTimerArmed_ = true;
    offlineStartedMs_ = millis();
    retryDelayMs_ = AppConfig::YORADIO_RECONNECT_MIN_MS;
    Serial.printf("YORADIO HOST: %s:%u\n", AppConfig::YORADIO_HOST,
                  AppConfig::YORADIO_PORT);
    if (AppConfig::YORADIO_HOST[0] == '\0') {
        phase_ = Phase::DISABLED;
        Serial.println("YORADIO DISABLED: brak hosta");
        return;
    }
    socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    socket_.setReconnectInterval(AppConfig::YORADIO_RECONNECT_MAX_MS + 1000UL);
}

void RadioService::update(AppState& state) {
    state_ = &state;
    if (phase_ == Phase::DISABLED) return;

    if (WiFi.status() != WL_CONNECTED) {
        if (phase_ != Phase::IDLE) {
            manualDisconnect_ = true;
            socket_.disconnect();
            socketStarted_ = false;
            phase_ = Phase::IDLE;
            setOffline(true);
        }
        updateOfflineErrorUi();
        return;
    }

    if (phase_ == Phase::IDLE) {
        backoffStep_ = 0;
        startSession();
        return;
    }
    if (socketStarted_) socket_.loop();

    const uint32_t now = millis();
    if (phase_ == Phase::CONNECTING &&
        now - phaseStartedMs_ >= AppConfig::YORADIO_CONNECT_TIMEOUT_MS) {
        manualDisconnect_ = true;
        socket_.disconnect();
        socketStarted_ = false;
        setOffline(true);
        scheduleRetry();
    } else if (phase_ == Phase::WAIT_FIRST_DATA &&
               now - phaseStartedMs_ >=
                   AppConfig::YORADIO_FIRST_DATA_TIMEOUT_MS) {
        manualDisconnect_ = true;
        socket_.disconnect();
        socketStarted_ = false;
        setOffline(true);
        scheduleRetry();
    } else if (phase_ == Phase::ONLINE &&
               now - lastDataMs_ >= AppConfig::YORADIO_SILENCE_TIMEOUT_MS) {
        if (socket_.sendTXT("getindex=1")) {
            phase_ = Phase::WAIT_REFRESH_DATA;
            phaseStartedMs_ = now;
            Serial.println("YORADIO SILENCE: getindex=1");
        }
    } else if (phase_ == Phase::WAIT_REFRESH_DATA &&
               now - phaseStartedMs_ >=
                   AppConfig::YORADIO_FIRST_DATA_TIMEOUT_MS) {
        manualDisconnect_ = true;
        socket_.disconnect();
        socketStarted_ = false;
        setOffline(true);
        scheduleRetry();
    } else if (phase_ == Phase::WAIT_RETRY &&
               now - phaseStartedMs_ >= retryDelayMs_) {
        startSession();
    }

    updateOfflineErrorUi();

}

void RadioService::previous() { sendCommand("prev=1", true); }
void RadioService::next() { sendCommand("next=1", true); }
void RadioService::togglePlay() { sendCommand("toggle=1", false); }
void RadioService::volumeDown() { sendCommand("volm=1", false); }
void RadioService::volumeUp() { sendCommand("volp=1", false); }

void RadioService::startSession() {
    manualDisconnect_ = false;
    phase_ = Phase::CONNECTING;
    phaseStartedMs_ = millis();
    lastDataMs_ = 0;
#ifdef YORADIO_RX_DIAGNOSTICS
    diagnosticFrameCount_ = 0;
#endif
    socketStarted_ = true;
    Serial.println("YORADIO CONNECTING");
    socket_.begin(AppConfig::YORADIO_HOST, AppConfig::YORADIO_PORT,
                  AppConfig::YORADIO_PATH);
}

void RadioService::handleEvent(WStype_t type, uint8_t* payload,
                               size_t length) {
    if (!state_) return;
    switch (type) {
        case WStype_CONNECTED:
            phase_ = Phase::WAIT_FIRST_DATA;
            phaseStartedMs_ = millis();
            Serial.println("YORADIO CONNECTED");
            socket_.sendTXT("getindex=1");
            break;
        case WStype_TEXT:
            {
            const bool firstData = !state_->radioOnline;
            if ((phase_ == Phase::WAIT_FIRST_DATA || phase_ == Phase::ONLINE ||
                 phase_ == Phase::WAIT_REFRESH_DATA) &&
                processMessage(payload, length)) {
                lastDataMs_ = millis();
                phaseStartedMs_ = lastDataMs_;
                phase_ = Phase::ONLINE;
                backoffStep_ = 0;
                if (firstData) Serial.println("YORADIO FIRST DATA");
            }
            break;
            }
        case WStype_DISCONNECTED:
        case WStype_ERROR:
            socketStarted_ = false;
            if (manualDisconnect_) {
                manualDisconnect_ = false;
                break;
            }
            setOffline(true);
            Serial.println("YORADIO DISCONNECTED");
            if (WiFi.status() == WL_CONNECTED) scheduleRetry();
            else phase_ = Phase::IDLE;
            break;
        default:
            break;
    }
}

bool RadioService::processMessage(const uint8_t* payload, size_t length) {
#ifdef YORADIO_RX_DIAGNOSTICS
    if (diagnosticFrameCount_ < 20) {
        Serial.print("YORADIO RX: ");
        Serial.write(payload, length);
        Serial.println();
        ++diagnosticFrameCount_;
    }
#endif
    StaticJsonDocument<1024> document;
    if (deserializeJson(document, payload, length) ||
        !document.is<JsonObject>()) {
        return false;
    }
    const JsonArrayConst entries = document["payload"].as<JsonArrayConst>();

    bool changed = false;
    for (JsonObjectConst entry : entries) {
        const char* id = entry["id"] | "";
        const JsonVariantConst value = entry["value"];
        if (strcmp(id, "nameset") == 0 && value.is<const char*>()) {
            changed |= setString(state_->radioStation,
                                 String(value.as<const char*>()));
        } else if (strcmp(id, "meta") == 0 && value.is<const char*>()) {
            changed |= applyMeta(*state_, value.as<const char*>());
        } else if (strcmp(id, "playerwrap") == 0 &&
                   value.is<const char*>()) {
            String playback = value.as<const char*>();
            playback.trim();
            playback.toLowerCase();
            const bool playing = playback == "play" ||
                                 playback == "playing" ||
                                 playback == "radio";
            if (state_->radioPlaying != playing) {
                state_->radioPlaying = playing;
                changed = true;
            }
        } else if (strcmp(id, "volume") == 0) {
            int volume = 0;
            if (readInt(value, volume) && volume >= 0 && volume <= 254) {
                if (state_->radioVolume != volume) {
                    state_->radioVolume = volume;
                    changed = true;
                }
            }
        } else if (strcmp(id, "bitrate") == 0) {
            int bitrate = 0;
            if (readInt(value, bitrate) && bitrate >= 0) {
                if (state_->radioBitrate != bitrate) {
                    state_->radioBitrate = bitrate;
                    changed = true;
                }
            }
        }
    }
    if (!state_->radioOnline) {
        state_->radioOnline = true;
        state_->radioOfflineError = false;
        offlineTimerArmed_ = false;
        offlineStartedMs_ = 0;
        changed = true;
    }
    if (changed) ++state_->radioRevision;
    return true;
}

bool RadioService::sendCommand(const char* command, bool clearMedia) {
    if (phase_ != Phase::ONLINE || !socketStarted_ ||
        !state_ || !state_->radioOnline || !socket_.isConnected()) {
        return false;
    }
    if (!socket_.sendTXT(command)) return false;
    if (clearMedia) {
        bool changed = setString(state_->radioStation, "---");
        changed |= setString(state_->radioArtist, "");
        changed |= setString(state_->radioTitle, "---");
        if (changed) ++state_->radioRevision;
    }
    return true;
}

void RadioService::setOffline(bool clearMedia) {
    if (!state_) return;
    bool changed = false;
    if (state_->radioOnline) {
        state_->radioOnline = false;
        changed = true;
    }
    if (!offlineTimerArmed_) {
        offlineTimerArmed_ = true;
        offlineStartedMs_ = millis();
    }
    if (state_->radioPlaying) {
        state_->radioPlaying = false;
        changed = true;
    }
    if (clearMedia) {
        changed |= setString(state_->radioStation, "---");
        changed |= setString(state_->radioArtist, "");
        changed |= setString(state_->radioTitle, "---");
    }
    if (changed) ++state_->radioRevision;
}

void RadioService::updateOfflineErrorUi() {
    if (!state_) return;

    if (state_->radioOnline) {
        if (state_->radioOfflineError) {
            state_->radioOfflineError = false;
            ++state_->radioRevision;
        }
        offlineTimerArmed_ = false;
        offlineStartedMs_ = 0;
        return;
    }

    if (!offlineTimerArmed_) {
        offlineTimerArmed_ = true;
        offlineStartedMs_ = millis();
    }

    if (!state_->radioOfflineError &&
        millis() - offlineStartedMs_ >= AppConfig::RADIO_OFFLINE_UI_TIMEOUT_MS) {
        state_->radioOfflineError = true;
        ++state_->radioRevision;
    }
}

void RadioService::scheduleRetry() {
    static const uint8_t multipliers[] = {1, 2, 5, 10, 30};
    const uint8_t index = min<uint8_t>(backoffStep_, 4);
    retryDelayMs_ = min(
        AppConfig::YORADIO_RECONNECT_MIN_MS * multipliers[index],
        AppConfig::YORADIO_RECONNECT_MAX_MS);
    if (backoffStep_ < 4) ++backoffStep_;
    phase_ = Phase::WAIT_RETRY;
    phaseStartedMs_ = millis();
    Serial.printf("YORADIO RETRY: %lu ms\n",
                  static_cast<unsigned long>(retryDelayMs_));
}
