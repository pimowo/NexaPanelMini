#pragma once
#include "core/Navigation.h"
#include "core/AppState.h"
#include "display/DisplayDriver.h"
#include "display/TouchDriver.h"
#include "ui/BottomBar.h"
#include "ui/HomeScreen.h"
#include "ui/RadioScreen.h"
#include "ui/BoilerScreen.h"
#include "ui/WeatherScreen.h"

class UiManager {
public:
    void begin(DisplayDriver& display, TouchDriver& touch, Navigation& navigation);
    void update(AppState& state);

private:
    void redraw();
    void handleTouch(const TouchPoint& point);

    DisplayDriver* display_ = nullptr;
    TouchDriver* touch_ = nullptr;
    Navigation* navigation_ = nullptr;
    AppState* state_ = nullptr;

    ScreenId lastScreen_ = ScreenId::HOME;
    bool redrawPending_ = true;
    bool touchDown_ = false;

    BottomBar bottomBar_;
    HomeScreen home_;
    RadioScreen radio_;
    BoilerScreen boiler_;
    WeatherScreen weather_;
};
