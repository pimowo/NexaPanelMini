#include "ui/WeatherScreen.h"
#include "display/Theme.h"

void WeatherScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void WeatherScreen::update(DisplayDriver& display, const AppState& state) {
    if (revision_ == state.weatherRevision) return;
    display.tft().fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void WeatherScreen::drawContent(DisplayDriver& display, const AppState& state) {

    display.drawUtf8("SZCZEGÓŁY POGODY", 120, 24, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);

    for (int i = 0; i < 5; ++i) {
        int y = 62 + i * 38;
        const auto& d = state.forecast[i];

        String line = d.dayName + "  ";
        line += isnan(d.tempMax) ? "--" : String(d.tempMax, 0);
        line += "/";
        line += isnan(d.tempMin) ? "--" : String(d.tempMin, 0);
        line += " C  " + d.condition;

        display.drawUtf8(line, 12, y, 2,
                         Theme::TEXT, Theme::BG, ML_DATUM);
    }
    revision_ = state.weatherRevision;
}
