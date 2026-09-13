#pragma once
#include "core/AppState.h"

class RadioService {
public:
    void begin();
    void update(AppState& state);

    void previous();
    void next();
    void togglePlay();
    void volumeDown();
    void volumeUp();
};
