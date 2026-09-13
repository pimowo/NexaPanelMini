#pragma once
#include "ui/ScreenBase.h"

class HomeScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;
    void onTouch(const TouchPoint& point) override;

private:
    void drawTime(DisplayDriver& display, const AppState& state);
    void drawWeather(DisplayDriver& display, const AppState& state);

    uint32_t timeRevision_ = UINT32_MAX;
    uint32_t weatherRevision_ = UINT32_MAX;
};
