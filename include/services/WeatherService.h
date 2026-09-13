#pragma once
#include "core/AppState.h"

class WeatherService {
public:
    void begin();
    void update(AppState& state);
};
