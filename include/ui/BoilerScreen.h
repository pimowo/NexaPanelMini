#pragma once
#include "ui/ScreenBase.h"

enum class BoilerAction : uint8_t {
    NONE,
    TOGGLE_POWER,
    TOGGLE_HVAC,
    OPEN_TEMPERATURE,
    COMFORT,
    SLEEP
};

class BoilerScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;
    BoilerAction actionAt(const TouchPoint& point) const;

private:
    void drawStatic(DisplayDriver& display);
    void drawPower(DisplayDriver& display, const AppState& state,
                   bool clearRegion = true);
    void drawFlame(DisplayDriver& display, const AppState& state,
                   bool clearRegion = true);
    void drawRadiator(DisplayDriver& display, const AppState& state,
                      bool clearRegion = true);
    void drawCurrentTemperature(DisplayDriver& display, const AppState& state,
                                bool clearRegion = true);
    void drawTargetTemperature(DisplayDriver& display, const AppState& state,
                               bool clearRegion = true);
    void drawPreset(DisplayDriver& display, const AppState& state,
                    bool clearRegion = true);
    void cacheState(const AppState& state);

    uint32_t revision_ = UINT32_MAX;
    bool online_ = false;
    bool powerOn_ = false;
    bool heating_ = false;
    bool hvacHeat_ = false;
    BoilerPreset preset_ = BoilerPreset::MANUAL;
    float currentTemperature_ = NAN;
    float targetTemperature_ = NAN;
    bool cacheValid_ = false;
};
