#pragma once
#include <XPT2046_Touchscreen.h>
#include "pins.h"

struct TouchPoint {
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
};

class TouchDriver {
public:
    TouchDriver();
    void begin();
    TouchPoint read();

private:
    XPT2046_Touchscreen touch_;
};
