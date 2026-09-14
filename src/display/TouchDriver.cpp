#include "display/TouchDriver.h"
#include "config.h"

TouchDriver::TouchDriver()
    : touch_(PIN_TOUCH_CS) {}

void TouchDriver::begin() {
    touch_.begin();
    // Rotation 1 leaves the library's measured X/Y axes unchanged.
    touch_.setRotation(1);
    Serial.println("TOUCH XPT2046: ready, calibrated");
}

TouchPoint TouchDriver::read() {
    TouchPoint out;
    if (!touch_.touched()) {
        return out;
    }

    const TS_Point raw = touch_.getPoint();

    const int32_t screenX = map(raw.y,
                                AppConfig::TOUCH_Y_MAX,
                                AppConfig::TOUCH_Y_MIN,
                                0, 239);
    const int32_t screenY = map(raw.x,
                                AppConfig::TOUCH_X_MIN,
                                AppConfig::TOUCH_X_MAX,
                                0, 319);

    out.x = static_cast<int16_t>(constrain(screenX, 0L, 239L));
    out.y = static_cast<int16_t>(constrain(screenY, 0L, 319L));
    out.touched = true;
    return out;
}
