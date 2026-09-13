#pragma once
#include "core/AppState.h"
#include "display/DisplayDriver.h"
#include "display/TouchDriver.h"

class ScreenBase {
public:
    virtual ~ScreenBase() = default;

    virtual void enter() {}
    virtual void draw(DisplayDriver& display, const AppState& state) = 0;
    virtual void update(DisplayDriver& display, const AppState& state) {}
    virtual void onTouch(const TouchPoint& point) {}
};
