#pragma once
#include "ui/ScreenBase.h"

enum class BoilerTemperatureAction : uint8_t {
    NONE,
    DECREASE,
    INCREASE,
    BACK
};

class BoilerTemperatureScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;

    BoilerTemperatureAction actionAt(const TouchPoint& point) const;
    void setLocalTarget(float target);
    float displayedTarget() const;

private:
    void drawStatic(DisplayDriver& display);
    void drawControls(DisplayDriver& display);
    void drawTarget(DisplayDriver& display, bool clearRegion = true);
    void syncFromState(const AppState& state);

    float displayedTarget_ = NAN;
    float lastStateTarget_ = NAN;
    bool localOverride_ = false;
    bool cacheValid_ = false;
};
