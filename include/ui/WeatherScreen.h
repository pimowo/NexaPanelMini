#pragma once
#include "ui/ScreenBase.h"

class WeatherScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;

private:
    void drawStatic(DisplayDriver& display);
    void drawDay(DisplayDriver& display, const WeatherDay& day,
                 uint8_t index, bool clearRegion = true);

    WeatherDay days_[3];
    bool cacheValid_ = false;
};
