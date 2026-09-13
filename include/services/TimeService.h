#pragma once
#include "core/AppState.h"

class TimeService {
public:
    void begin();
    void update(AppState& state);

private:
    unsigned long lastFormatMs_ = 0;
    bool synchronizationReported_ = false;
};
