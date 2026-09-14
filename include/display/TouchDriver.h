#pragma once
#include <XPT2046_Touchscreen.h>
#include "display/DisplayDriver.h"
#include "pins.h"

struct TouchPoint {
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
};

class TouchDriver {
public:
    struct Calibration {
        int16_t xMin = 0;
        int16_t xMax = 0;
        int16_t yMin = 0;
        int16_t yMax = 0;
        bool swapAxes = false;
        bool invertX = false;
        bool invertY = false;
    };

    TouchDriver();
    void begin();
    TouchPoint read();
    bool hasCalibration() const;
    bool loadCalibration();
    bool calibrate(DisplayDriver& display);
    bool detectBootCalibrationHold(DisplayDriver& display,
                                   uint32_t holdMs = 3000UL);

private:
    bool readRawMedian(TS_Point& out, uint8_t samples);
    bool waitForPressSample(TS_Point& out);
    bool waitForRelease(uint32_t timeoutMs);
    TouchPoint mapToScreen(const TS_Point& raw) const;
    bool isCalibrationSane(const Calibration& candidate) const;

    XPT2046_Touchscreen touch_;
    Calibration calibration_;
    bool calibrated_ = false;
};
