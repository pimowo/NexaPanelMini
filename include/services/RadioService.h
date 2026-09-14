#pragma once
#include <WebSocketsClient.h>
#include "core/AppState.h"

class RadioService {
public:
    void begin(AppState& state);
    void update(AppState& state);

    void previous();
    void next();
    void togglePlay();
    void volumeDown();
    void volumeUp();

private:
    enum class Phase : uint8_t {
        DISABLED,
        IDLE,
        CONNECTING,
        WAIT_FIRST_DATA,
        ONLINE,
        WAIT_REFRESH_DATA,
        WAIT_RETRY
    };

    void startSession();
    void handleEvent(WStype_t type, uint8_t* payload, size_t length);
    bool processMessage(const uint8_t* payload, size_t length);
    bool sendCommand(const char* command, bool clearMedia);
    void setOffline(bool clearMedia);
    void scheduleRetry();
    void updateOfflineErrorUi();

    AppState* state_ = nullptr;
    WebSocketsClient socket_;
    Phase phase_ = Phase::IDLE;
    uint32_t phaseStartedMs_ = 0;
    uint32_t lastDataMs_ = 0;
    uint32_t retryDelayMs_ = 0;
    uint8_t backoffStep_ = 0;
    bool socketStarted_ = false;
    bool manualDisconnect_ = false;
    bool offlineTimerArmed_ = false;
    uint32_t offlineStartedMs_ = 0;
#ifdef YORADIO_RX_DIAGNOSTICS
    uint8_t diagnosticFrameCount_ = 0;
#endif
};
