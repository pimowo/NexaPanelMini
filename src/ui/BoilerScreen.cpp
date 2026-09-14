#include "ui/BoilerScreen.h"
#include "ui/BottomBar.h"
#include "display/Theme.h"
#include <math.h>

namespace {
constexpr int16_t STATUS_Y = 4;
constexpr int16_t STATUS_HEIGHT = 52;
constexpr int16_t PRESET_Y = 218;
constexpr uint16_t ACTIVE_GREEN = 0x07E0;
constexpr uint16_t FLAME_ORANGE = 0xFD20;
constexpr uint16_t SUN_YELLOW = 0xFFE0;
constexpr uint16_t SLEEP_BLUE = 0x04FF;

bool sameTemperature(float first, float second) {
    return isnan(first) == isnan(second) &&
           (isnan(first) || fabsf(first - second) < 0.05F);
}

void drawPowerIcon(TFT_eSPI& tft, int16_t cx, int16_t cy,
                   uint16_t color) {
    tft.drawCircle(cx, cy + 2, 12, color);
    tft.drawCircle(cx, cy + 2, 11, color);
    tft.fillRect(cx - 2, cy - 14, 5, 17, Theme::BG);
    tft.drawFastVLine(cx, cy - 14, 17, color);
}

void drawFlameIcon(TFT_eSPI& tft, int16_t cx, int16_t cy,
                   uint16_t color) {
    tft.fillTriangle(cx, cy - 15, cx - 11, cy + 10,
                     cx + 11, cy + 10, color);
    tft.fillCircle(cx, cy + 7, 11, color);
    tft.fillTriangle(cx + 1, cy - 4, cx - 4, cy + 10,
                     cx + 6, cy + 10, Theme::BG);
}

void drawRadiatorIcon(TFT_eSPI& tft, int16_t x, int16_t y,
                      uint16_t color) {
    for (uint8_t index = 0; index < 4; ++index) {
        tft.drawRoundRect(x + index * 7, y, 5, 25, 2, color);
    }
    tft.drawFastHLine(x - 3, y + 4, 3, color);
    tft.drawFastHLine(x + 26, y + 20, 4, color);
}

void drawSunIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
    tft.fillCircle(cx, cy, 7, color);
    tft.drawFastHLine(cx - 12, cy, 5, color);
    tft.drawFastHLine(cx + 8, cy, 5, color);
    tft.drawFastVLine(cx, cy - 12, 5, color);
    tft.drawFastVLine(cx, cy + 8, 5, color);
}

void drawMoonIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color,
                  uint16_t background) {
    tft.fillCircle(cx, cy, 10, color);
    tft.fillCircle(cx + 5, cy - 4, 9, background);
}
}

void BoilerScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawStatic(display);
    drawPower(display, state, false);
    drawFlame(display, state, false);
    drawRadiator(display, state, false);
    drawCurrentTemperature(display, state, false);
    drawTargetTemperature(display, state, false);
    drawPreset(display, state, false);
    cacheState(state);
}

void BoilerScreen::update(DisplayDriver& display, const AppState& state) {
    if (revision_ == state.boilerRevision) return;
    if (!cacheValid_ || online_ != state.boilerOnline ||
        powerOn_ != state.boilerPowerOn) {
        drawPower(display, state);
    }
    if (!cacheValid_ || online_ != state.boilerOnline ||
        heating_ != state.boilerEnabled) {
        drawFlame(display, state);
    }
    if (!cacheValid_ || online_ != state.boilerOnline ||
        hvacHeat_ != state.boilerHvacHeat) {
        drawRadiator(display, state);
    }
    if (!cacheValid_ ||
        !sameTemperature(currentTemperature_, state.boilerCurrentTemp)) {
        drawCurrentTemperature(display, state);
    }
    if (!cacheValid_ ||
        !sameTemperature(targetTemperature_, state.boilerTargetTemp)) {
        drawTargetTemperature(display, state);
    }
    if (!cacheValid_ || online_ != state.boilerOnline ||
        comfort_ != state.boilerComfortMode) {
        drawPreset(display, state);
    }
    cacheState(state);
}

void BoilerScreen::drawStatic(DisplayDriver& display) {
    display.drawUtf8("Temperatura", 120, 82, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
    display.drawUtf8("Aktualna", 12, 128, 2,
                     Theme::TEXT, Theme::BG, ML_DATUM);
    display.drawUtf8("Zadana", 12, 172, 2,
                     Theme::ACCENT, Theme::BG, ML_DATUM);
}

void BoilerScreen::drawPower(DisplayDriver& display, const AppState& state,
                             bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(4, STATUS_Y, 72, STATUS_HEIGHT, Theme::BG);
    const uint16_t color = !state.boilerOnline ? Theme::DIM
        : state.boilerPowerOn ? ACTIVE_GREEN : Theme::DIM;
    tft.drawRect(4, STATUS_Y, 72, STATUS_HEIGHT, color);
    drawPowerIcon(tft, 40, STATUS_Y + 26, color);
}

void BoilerScreen::drawFlame(DisplayDriver& display, const AppState& state,
                             bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(84, STATUS_Y, 72, STATUS_HEIGHT, Theme::BG);
    const uint16_t color = state.boilerOnline && state.boilerEnabled
        ? FLAME_ORANGE : Theme::DIM;
    tft.drawRect(84, STATUS_Y, 72, STATUS_HEIGHT, Theme::DIM);
    drawFlameIcon(tft, 120, STATUS_Y + 26, color);
}

void BoilerScreen::drawRadiator(DisplayDriver& display,
                                const AppState& state, bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(164, STATUS_Y, 72, STATUS_HEIGHT, Theme::BG);
    const uint16_t color = !state.boilerOnline ? Theme::DIM
        : state.boilerHvacHeat ? ACTIVE_GREEN : Theme::DIM;
    tft.drawRect(164, STATUS_Y, 72, STATUS_HEIGHT, color);
    drawRadiatorIcon(tft, 185, STATUS_Y + 14, color);
}

void BoilerScreen::drawCurrentTemperature(DisplayDriver& display,
                                          const AppState& state,
                                          bool clearRegion) {
    if (clearRegion) display.tft().fillRect(102, 108, 134, 42, Theme::BG);
    const String value = isnan(state.boilerCurrentTemp)
        ? "--.-°C" : String(state.boilerCurrentTemp, 1) + "°C";
    display.drawUtf8(value, 218, 128, 4,
                     Theme::TEXT, Theme::BG, MR_DATUM);
}

void BoilerScreen::drawTargetTemperature(DisplayDriver& display,
                                         const AppState& state,
                                         bool clearRegion) {
    if (clearRegion) display.tft().fillRect(102, 152, 134, 42, Theme::BG);
    const String value = isnan(state.boilerTargetTemp)
        ? "--.-°C" : String(state.boilerTargetTemp, 1) + "°C";
    display.drawUtf8(value, 218, 172, 4,
                     Theme::ACCENT, Theme::BG, MR_DATUM);
}

void BoilerScreen::drawPreset(DisplayDriver& display, const AppState& state,
                              bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(4, PRESET_Y, 232, 44, Theme::BG);
    const uint16_t comfortColor = !state.boilerOnline ? Theme::DIM
        : state.boilerComfortMode ? SUN_YELLOW : Theme::DIM;
    const uint16_t sleepColor = !state.boilerOnline ? Theme::DIM
        : state.boilerComfortMode ? Theme::DIM : SLEEP_BLUE;
    tft.drawRect(4, PRESET_Y, 112, 44, comfortColor);
    tft.drawRect(124, PRESET_Y, 112, 44, sleepColor);
    drawSunIcon(tft, 25, PRESET_Y + 22, comfortColor);
    drawMoonIcon(tft, 145, PRESET_Y + 22, sleepColor, Theme::BG);
    display.drawUtf8("KOMFORT", 72, PRESET_Y + 22, 2,
                     comfortColor, Theme::BG, MC_DATUM);
    display.drawUtf8("SEN", 190, PRESET_Y + 22, 2,
                     sleepColor, Theme::BG, MC_DATUM);
}

void BoilerScreen::cacheState(const AppState& state) {
    online_ = state.boilerOnline;
    powerOn_ = state.boilerPowerOn;
    heating_ = state.boilerEnabled;
    hvacHeat_ = state.boilerHvacHeat;
    comfort_ = state.boilerComfortMode;
    currentTemperature_ = state.boilerCurrentTemp;
    targetTemperature_ = state.boilerTargetTemp;
    revision_ = state.boilerRevision;
    cacheValid_ = true;
}

BoilerAction BoilerScreen::actionAt(const TouchPoint& point) const {
    if (!point.touched || point.y < 0 || point.y >= BottomBar::Y) {
        return BoilerAction::NONE;
    }
    if (point.y >= STATUS_Y && point.y < STATUS_Y + STATUS_HEIGHT) {
        if (point.x >= 4 && point.x < 76) return BoilerAction::TOGGLE_POWER;
        if (point.x >= 164 && point.x < 236) return BoilerAction::TOGGLE_HVAC;
    }
    if (point.y >= PRESET_Y && point.y < PRESET_Y + 44) {
        if (point.x >= 4 && point.x < 116) return BoilerAction::COMFORT;
        if (point.x >= 124 && point.x < 236) return BoilerAction::SLEEP;
    }
    return BoilerAction::NONE;
}
