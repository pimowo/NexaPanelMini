#include "ui/HomeScreen.h"
#include "display/Theme.h"

void HomeScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);

    drawTime(display, state);
    drawWeather(display, state);
}

void HomeScreen::update(DisplayDriver& display, const AppState& state) {
    if (timeRevision_ != state.timeRevision) drawTime(display, state);
    if (weatherRevision_ != state.weatherRevision) drawWeather(display, state);
}

void HomeScreen::drawTime(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 16, 240, 112, Theme::BG);

    display.drawUtf8(state.timeText, 120, 48, 6,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.weekdayText, 120, 92, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.dateText, 120, 114, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);

    timeRevision_ = state.timeRevision;
}

void HomeScreen::drawWeather(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 130, 240, 136, Theme::BG);

    display.drawUtf8(state.weatherText, 120, 170, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);

    String temp = isnan(state.outsideTemp) ? "--.- C" : String(state.outsideTemp, 1) + " C";
    display.drawUtf8(temp, 120, 200, 4,
                     Theme::ACCENT, Theme::BG, MC_DATUM);

    display.drawUtf8("Dotknij pogody: prognoza 5 dni", 120, 242, 1,
                     Theme::DIM, Theme::BG, MC_DATUM);

    weatherRevision_ = state.weatherRevision;
}

void HomeScreen::onTouch(const TouchPoint& point) {
    (void)point;
    // TODO: UiManager obsłuży kliknięcie obszaru pogody -> WEATHER_DETAILS.
}
