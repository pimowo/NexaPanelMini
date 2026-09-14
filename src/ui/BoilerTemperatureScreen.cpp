#include "ui/BoilerTemperatureScreen.h"
#include "display/Theme.h"
#include <math.h>

namespace {
constexpr int16_t MINUS_X = 4;
constexpr int16_t MINUS_Y = 84;
constexpr int16_t MINUS_W = 72;
constexpr int16_t MINUS_H = 132;
constexpr int16_t PLUS_X = 164;
constexpr int16_t PLUS_Y = 84;
constexpr int16_t PLUS_W = 72;
constexpr int16_t PLUS_H = 132;

constexpr int16_t TARGET_X = 84;
constexpr int16_t TARGET_Y = 116;
constexpr int16_t TARGET_W = 72;
constexpr int16_t TARGET_H = 64;

constexpr int16_t BACK_X = 30;
constexpr int16_t BACK_Y = 272;
constexpr int16_t BACK_W = 180;
constexpr int16_t BACK_H = 40;

bool sameTemperature(float first, float second) {
    return isnan(first) == isnan(second) &&
           (isnan(first) || fabsf(first - second) < 0.05F);
}
}

void BoilerTemperatureScreen::draw(DisplayDriver& display,
                                   const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 320, Theme::BG);

    syncFromState(state);
    drawStatic(display);
    drawControls(display);
    drawTarget(display, false);
    cacheValid_ = true;
}

void BoilerTemperatureScreen::update(DisplayDriver& display,
                                     const AppState& state) {
    const float before = displayedTarget_;
    syncFromState(state);
    if (!cacheValid_ || !sameTemperature(before, displayedTarget_)) {
        drawTarget(display);
    }
    cacheValid_ = true;
}

BoilerTemperatureAction BoilerTemperatureScreen::actionAt(
    const TouchPoint& point) const {
    if (!point.touched) return BoilerTemperatureAction::NONE;

    if (point.x >= BACK_X && point.x < BACK_X + BACK_W &&
        point.y >= BACK_Y && point.y < BACK_Y + BACK_H) {
        return BoilerTemperatureAction::BACK;
    }
    if (point.x >= MINUS_X && point.x < MINUS_X + MINUS_W &&
        point.y >= MINUS_Y && point.y < MINUS_Y + MINUS_H) {
        return BoilerTemperatureAction::DECREASE;
    }
    if (point.x >= PLUS_X && point.x < PLUS_X + PLUS_W &&
        point.y >= PLUS_Y && point.y < PLUS_Y + PLUS_H) {
        return BoilerTemperatureAction::INCREASE;
    }
    return BoilerTemperatureAction::NONE;
}

void BoilerTemperatureScreen::setLocalTarget(float target) {
    displayedTarget_ = target;
    localOverride_ = true;
}

float BoilerTemperatureScreen::displayedTarget() const {
    return displayedTarget_;
}

void BoilerTemperatureScreen::drawStatic(DisplayDriver& display) {
    display.drawUtf8("Temperatura", 120, 22, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
}

void BoilerTemperatureScreen::drawControls(DisplayDriver& display) {
    auto& tft = display.tft();

    tft.drawRect(MINUS_X, MINUS_Y, MINUS_W, MINUS_H, Theme::DIM);
    tft.drawRect(PLUS_X, PLUS_Y, PLUS_W, PLUS_H, Theme::DIM);

    display.drawUtf8("-", MINUS_X + MINUS_W / 2, MINUS_Y + MINUS_H / 2, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8("+", PLUS_X + PLUS_W / 2, PLUS_Y + PLUS_H / 2, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);

    tft.drawRect(BACK_X, BACK_Y, BACK_W, BACK_H, Theme::DIM);
    display.drawUtf8("<- WRÓĆ", BACK_X + BACK_W / 2, BACK_Y + BACK_H / 2,
                     2, Theme::TEXT, Theme::BG, MC_DATUM);
}

void BoilerTemperatureScreen::drawTarget(DisplayDriver& display,
                                         bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, Theme::BG);
    const String value = isnan(displayedTarget_)
        ? "--.-°C"
        : String(displayedTarget_, 1) + "°C";
    display.drawUtf8(value, 120, 148, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
}

void BoilerTemperatureScreen::syncFromState(const AppState& state) {
    if (!isfinite(state.boilerTargetTemp)) return;

    if (!cacheValid_) {
        displayedTarget_ = state.boilerTargetTemp;
        lastStateTarget_ = state.boilerTargetTemp;
        localOverride_ = false;
        return;
    }

    if (!sameTemperature(lastStateTarget_, state.boilerTargetTemp)) {
        lastStateTarget_ = state.boilerTargetTemp;
        displayedTarget_ = state.boilerTargetTemp;
        localOverride_ = false;
        return;
    }

    if (!localOverride_ && !sameTemperature(displayedTarget_, state.boilerTargetTemp)) {
        displayedTarget_ = state.boilerTargetTemp;
    }
}
