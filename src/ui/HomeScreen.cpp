#include "ui/HomeScreen.h"
#include "display/Theme.h"
#include "ui/WeatherVisual.h"
#include "config.h"
#include <math.h>

namespace {
bool sameTemperature(float first, float second) {
    return isnan(first) == isnan(second) &&
           (isnan(first) || fabsf(first - second) < 0.05F);
}

bool selectDisplayedTemperature(const AppState& state, float& temperature,
                                bool& fromHa) {
    const bool haFresh = state.haOutsideTempValid &&
        static_cast<uint32_t>(millis() - state.haOutsideTempLastUpdateMs) <
            AppConfig::HA_OUTSIDE_TEMP_STALE_MS;
    if (haFresh && isfinite(state.haOutsideTemp)) {
        temperature = state.haOutsideTemp;
        fromHa = true;
        return true;
    }
    if (state.weatherValid && isfinite(state.outsideTemp)) {
        temperature = state.outsideTemp;
        fromHa = false;
        return true;
    }
    temperature = NAN;
    fromHa = false;
    return false;
}
}

void HomeScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawClock(display, state, false);
    drawWeekday(display, state, false);
    drawDate(display, state, false);
    drawWeatherSummary(display, state, false);
    drawTemperature(display, state, false);
    cacheValid_ = true;
}

void HomeScreen::update(DisplayDriver& display, const AppState& state) {
    const bool weatherValidityChanged = !cacheValid_ ||
                                        weatherValid_ != state.weatherValid;
    if (!cacheValid_ || clock_ != state.timeText) drawClock(display, state);
    if (!cacheValid_ || weekday_ != state.weekdayText) drawWeekday(display, state);
    if (!cacheValid_ || date_ != state.dateText) drawDate(display, state);
    if (weatherValidityChanged ||
        weatherCode_ != state.currentWeatherCode ||
        !sameTemperature(todayMax_, state.todayMax) ||
        !sameTemperature(todayMin_, state.todayMin)) {
        drawWeatherSummary(display, state);
    }
    float selectedTemperature = NAN;
    bool selectedFromHa = false;
    selectDisplayedTemperature(state, selectedTemperature, selectedFromHa);
    if (weatherValidityChanged || selectedFromHa != temperatureFromHa_ ||
        !sameTemperature(temperature_, selectedTemperature)) {
        drawTemperature(display, state);
    }
    cacheValid_ = true;
}

void HomeScreen::drawClock(DisplayDriver& display, const AppState& state,
                           bool clearRegion) {
    if (clearRegion) display.tft().fillRect(0, 12, 240, 48, Theme::BG);
    display.drawUtf8(state.timeText, 120, 36, 6,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
    clock_ = state.timeText;
}

void HomeScreen::drawWeekday(DisplayDriver& display, const AppState& state,
                             bool clearRegion) {
    if (clearRegion) display.tft().fillRect(0, 60, 240, 28, Theme::BG);
    display.drawUtf8(state.weekdayText, 120, 74, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    weekday_ = state.weekdayText;
}

void HomeScreen::drawDate(DisplayDriver& display, const AppState& state,
                          bool clearRegion) {
    if (clearRegion) display.tft().fillRect(0, 88, 240, 26, Theme::BG);
    display.drawUtf8(state.dateText, 120, 100, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    date_ = state.dateText;
}

void HomeScreen::drawWeatherSummary(DisplayDriver& display,
                                    const AppState& state,
                                    bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(0, 116, 240, 94, Theme::BG);
    if (state.weatherValid) {
        drawWeatherIcon(tft, 17, 138, 40, state.currentWeatherCode);
        if (!isnan(state.todayMax) && !isnan(state.todayMin)) {
            const String range = String(state.todayMax, 0) + "°C / " +
                                 String(state.todayMin, 0) + "°C";
            display.drawUtf8(range, 218, 158, 4,
                             Theme::TEXT, Theme::BG, MR_DATUM);
        }
        tft.setViewport(12, 186, 216, 24, true);
        tft.setTextWrap(false, false);
        display.drawUtf8(weatherDescriptionPl(state.currentWeatherCode),
                         0, 12, 2, Theme::TEXT, Theme::BG, ML_DATUM);
        tft.setTextWrap(true, true);
        tft.resetViewport();
    }
    weatherValid_ = state.weatherValid;
    weatherCode_ = state.currentWeatherCode;
    todayMax_ = state.todayMax;
    todayMin_ = state.todayMin;
}

void HomeScreen::drawTemperature(DisplayDriver& display,
                                 const AppState& state,
                                 bool clearRegion) {
    if (clearRegion) display.tft().fillRect(0, 210, 240, 56, Theme::BG);
    float selectedTemperature = NAN;
    bool selectedFromHa = false;
    if (selectDisplayedTemperature(state, selectedTemperature, selectedFromHa)) {
        const String temperature = String(selectedTemperature, 1) + "°C";
        display.drawUtf8("Aktualnie", 12, 236, 2,
                         Theme::ACCENT, Theme::BG, ML_DATUM);
        display.drawUtf8(temperature, 188, 236, 4,
                         Theme::ACCENT, Theme::BG, MR_DATUM);
    }
    temperature_ = selectedTemperature;
    temperatureFromHa_ = selectedFromHa;
    weatherValid_ = state.weatherValid;
}

void HomeScreen::onTouch(const TouchPoint& point) {
    (void)point;
}
