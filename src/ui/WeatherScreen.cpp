#include "ui/WeatherScreen.h"
#include "display/Theme.h"
#include "ui/WeatherVisual.h"
#include <math.h>

namespace {
bool sameDay(const WeatherDay& first, const WeatherDay& second) {
    return first.dateText == second.dateText &&
           first.weatherCode == second.weatherCode &&
           isnan(first.tempMax) == isnan(second.tempMax) &&
           isnan(first.tempMin) == isnan(second.tempMin) &&
           (isnan(first.tempMax) || fabsf(first.tempMax - second.tempMax) < 0.05F) &&
           (isnan(first.tempMin) || fabsf(first.tempMin - second.tempMin) < 0.05F);
}
}

void WeatherScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawStatic(display);
    for (uint8_t index = 0; index < 3; ++index) {
        drawDay(display, state.forecast[index + 1], index, false);
        days_[index] = state.forecast[index + 1];
    }
    cacheValid_ = true;
}

void WeatherScreen::update(DisplayDriver& display, const AppState& state) {
    for (uint8_t index = 0; index < 3; ++index) {
        const WeatherDay& day = state.forecast[index + 1];
        if (!cacheValid_ || !sameDay(days_[index], day)) {
            drawDay(display, day, index);
            days_[index] = day;
        }
    }
    cacheValid_ = true;
}

void WeatherScreen::drawStatic(DisplayDriver& display) {
    display.drawUtf8("Prognoza pogody", 120, 22, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
}

void WeatherScreen::drawDay(DisplayDriver& display, const WeatherDay& day,
                            uint8_t index, bool clearRegion) {
    auto& tft = display.tft();
    const int16_t y = 44 + index * 72;
    if (clearRegion) tft.fillRect(0, y, 240, 70, Theme::BG);
    if (day.dateText.length() == 0 || isnan(day.tempMax) ||
        isnan(day.tempMin)) {
        return;
    }
    display.drawUtf8(day.dateText, 14, y + 12, 2,
                     Theme::TEXT, Theme::BG, ML_DATUM);
    drawWeatherIcon(tft, 26, y + 27, 32, day.weatherCode);
    const String temperatures = String(day.tempMax, 0) + "°C / " +
                                String(day.tempMin, 0) + "°C";
    display.drawUtf8(temperatures, 226, y + 45, 4,
                     Theme::TEXT, Theme::BG, MR_DATUM);
}
