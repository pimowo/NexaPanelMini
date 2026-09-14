#pragma once
#include "core/Navigation.h"
#include "core/AppState.h"
#include "display/DisplayDriver.h"
#include "display/TouchDriver.h"
#include "ui/BottomBar.h"
#include "ui/HomeScreen.h"
#include "ui/RadioScreen.h"
#include "ui/BoilerScreen.h"
#include "ui/BoilerTemperatureScreen.h"
#include "ui/WeatherScreen.h"
#include "services/RadioService.h"
#include "services/BoilerService.h"

class UiManager {
public:
    void begin(DisplayDriver& display, TouchDriver& touch,
               Navigation& navigation, RadioService& radioService,
               BoilerService& boilerService);
    void update(AppState& state);

private:
    void redraw();
    void handleTouch(const TouchPoint& point);
    void updateVolumeHold(const TouchPoint& point);
    void updateBoilerTemperatureHold(const TouchPoint& point);
    void adjustBoilerTarget(float delta);

    DisplayDriver* display_ = nullptr;
    TouchDriver* touch_ = nullptr;
    Navigation* navigation_ = nullptr;
    RadioService* radioService_ = nullptr;
    BoilerService* boilerService_ = nullptr;
    AppState* state_ = nullptr;

    ScreenId lastScreen_ = ScreenId::HOME;
    bool redrawPending_ = true;
    bool touchDown_ = false;
    RadioAction heldRadioAction_ = RadioAction::NONE;
    BoilerTemperatureAction heldBoilerTempAction_ =
        BoilerTemperatureAction::NONE;
    uint32_t nextVolumeRepeatMs_ = 0;
    uint32_t nextBoilerTempRepeatMs_ = 0;
    uint32_t lastTouchMs_ = 0;

    BottomBar bottomBar_;
    HomeScreen home_;
    RadioScreen radio_;
    BoilerScreen boiler_;
    BoilerTemperatureScreen boilerTemperature_;
    WeatherScreen weather_;
};
