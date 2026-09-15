#include "ui/HomeScreen.h"
#include "display/Theme.h"
#include "ui/WeatherVisual.h"
#include "config.h"
#include <math.h>

namespace {
constexpr int16_t WEATHER_BLOCK_SHIFT_Y = -9;
constexpr int16_t HOME_BOTTOM_SHIFT_Y = -5;
constexpr int16_t HOME_VALUES_X = 124;

bool sameTemperature(float first, float second) {
    return isnan(first) == isnan(second) &&
           (isnan(first) || fabsf(first - second) < 0.05F);
}

bool samePressure(float first, float second) {
    return isnan(first) == isnan(second) &&
           (isnan(first) || fabsf(first - second) < 0.5F);
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

bool selectDisplayedPressure(const AppState& state, float& pressure,
                             bool& fromHa) {
    const bool haFresh = state.haOutsidePressureValid &&
        static_cast<uint32_t>(millis() - state.haOutsidePressureLastUpdateMs) <
            AppConfig::HA_OUTSIDE_TEMP_STALE_MS;
    if (haFresh && isfinite(state.haOutsidePressure)) {
        pressure = state.haOutsidePressure;
        fromHa = true;
        return true;
    }
    if (state.weatherValid && isfinite(state.outsidePressure)) {
        pressure = state.outsidePressure;
        fromHa = false;
        return true;
    }
    pressure = NAN;
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
    drawBottomWeatherBlock(display, state, false);
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
    float selectedPressure = NAN;
    bool pressureFromHa = false;
    selectDisplayedPressure(state, selectedPressure, pressureFromHa);
    if (weatherValidityChanged || selectedFromHa != temperatureFromHa_ ||
        pressureFromHa != pressureFromHa_ ||
        !sameTemperature(temperature_, selectedTemperature) ||
        !samePressure(pressure_, selectedPressure)) {
        drawBottomWeatherBlock(display, state);
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
    display.drawUtf8(state.dateText, 120, 100, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    date_ = state.dateText;
}

void HomeScreen::drawWeatherSummary(DisplayDriver& display,
                                    const AppState& state,
                                    bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) tft.fillRect(0, 116 + WEATHER_BLOCK_SHIFT_Y, 240, 94,
                                  Theme::BG);
    if (state.weatherValid) {
        drawWeatherIcon(tft, 17, 138 + WEATHER_BLOCK_SHIFT_Y, 40,
                        state.currentWeatherCode);
        if (!isnan(state.todayMax) && !isnan(state.todayMin)) {
            const String range = String(state.todayMax, 0) + "°C / " +
                                 String(state.todayMin, 0) + "°C";
            display.drawUtf8(range, 218, 158 + WEATHER_BLOCK_SHIFT_Y, 4,
                             Theme::TEXT, Theme::BG, MR_DATUM);
        }
        tft.setViewport(12, 186 + WEATHER_BLOCK_SHIFT_Y, 216, 24, true);
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

void HomeScreen::drawBottomWeatherBlock(DisplayDriver& display,
                                        const AppState& state,
                                        bool clearRegion) {
    if (clearRegion) display.tft().fillRect(0, 199, 240, 67, Theme::BG);
    float selectedTemperature = NAN;
    bool temperatureFromHa = false;
    float selectedPressure = NAN;
    bool pressureFromHa = false;
    const bool hasTemperature =
        selectDisplayedTemperature(state, selectedTemperature,
                                   temperatureFromHa);
    const bool hasPressure =
        selectDisplayedPressure(state, selectedPressure, pressureFromHa);

    display.drawUtf8("Aktualnie", 12, 241 + HOME_BOTTOM_SHIFT_Y, 2,
                     Theme::ACCENT, Theme::BG, ML_DATUM);

    if (hasTemperature) {
        String temperature = String(selectedTemperature, 1) + "°C";
        if (!temperatureFromHa) temperature += "*";
        display.drawUtf8(temperature, HOME_VALUES_X, 229 + HOME_BOTTOM_SHIFT_Y,
                         4, Theme::ACCENT, Theme::BG, ML_DATUM);
    }

    String pressureText = hasPressure
        ? String(lroundf(selectedPressure)) + " hPa"
        : "--- hPa";
    if (hasPressure && !pressureFromHa) pressureText += "*";
    display.drawUtf8(pressureText, HOME_VALUES_X, 252 + HOME_BOTTOM_SHIFT_Y,
                     2, Theme::ACCENT, Theme::BG, ML_DATUM);

    temperature_ = selectedTemperature;
    pressure_ = selectedPressure;
    temperatureFromHa_ = temperatureFromHa;
    pressureFromHa_ = pressureFromHa;
    weatherValid_ = state.weatherValid;
}

void HomeScreen::onTouch(const TouchPoint& point) {
    (void)point;
}
