#pragma once
#include "display/DisplayDriver.h"
#include "display/TouchDriver.h"
#include "core/Navigation.h"

class BottomBar {
public:
    static constexpr int16_t HEIGHT = 54;
    static constexpr int16_t Y = 320 - HEIGHT;

    void update(DisplayDriver& display, MainSection active);
    bool handleTouch(const TouchPoint& point, Navigation& navigation);
    bool sectionAt(const TouchPoint& point, MainSection& section) const;

private:
    void drawSegment(DisplayDriver& display, MainSection section, bool active);
    int16_t segmentX(MainSection section) const;
    bool hitRadio(int16_t x, int16_t y) const;
    bool hitHome(int16_t x, int16_t y) const;
    bool hitBoiler(int16_t x, int16_t y) const;

    bool rendered_ = false;
    MainSection lastActiveSection_ = MainSection::HOME;
    uint32_t renderCount_ = 0;
};
