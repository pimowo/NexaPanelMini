#pragma once
#include "ui/ScreenBase.h"

class HomeScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;
    void onTouch(const TouchPoint& point) override;

private:
    void drawClock(DisplayDriver& display, const AppState& state,
                   bool clearRegion = true);
    void drawWeekday(DisplayDriver& display, const AppState& state,
                     bool clearRegion = true);
    void drawDate(DisplayDriver& display, const AppState& state,
                  bool clearRegion = true);
    void drawWeatherSummary(DisplayDriver& display, const AppState& state,
                            bool clearRegion = true);
    void drawTemperature(DisplayDriver& display, const AppState& state,
                         bool clearRegion = true);

    String clock_;
    String weekday_;
    String date_;
    float temperature_ = NAN;
    bool temperatureFromHa_ = false;
    float todayMax_ = NAN;
    float todayMin_ = NAN;
    int weatherCode_ = -2;
    bool weatherValid_ = false;
    bool cacheValid_ = false;
};
