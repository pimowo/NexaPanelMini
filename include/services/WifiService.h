#pragma once
#include "core/AppState.h"

class WifiService {
public:
    void begin();
    void update(AppState& state);

private:
    enum class Phase : uint8_t {
        DISABLED,
        CONNECTING,
        CONNECTED,
        WAIT_RETRY
    };

    void startConnection();
    void scheduleRetry();

    Phase phase_ = Phase::DISABLED;
    uint32_t phaseStartedMs_ = 0;
    uint32_t reconnectDelayMs_ = 0;
};
