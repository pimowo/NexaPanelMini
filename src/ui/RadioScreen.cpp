#include "ui/RadioScreen.h"
#include "display/Theme.h"

void RadioScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void RadioScreen::update(DisplayDriver& display, const AppState& state) {
    if (revision_ == state.radioRevision) return;
    display.tft().fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void RadioScreen::drawContent(DisplayDriver& display, const AppState& state) {

    display.drawUtf8("yoRadio", 120, 28, 4,
                     Theme::ACCENT, Theme::BG, MC_DATUM);

    display.drawUtf8(state.radioStation, 120, 74, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.radioArtist, 120, 108, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.radioTitle, 120, 136, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);

    display.drawUtf8("<", 42, 205, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.radioPlaying ? "||" : ">", 120, 205, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(">", 198, 205, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);

    display.drawUtf8("-   VOL " + String(state.radioVolume) + "   +",
                     120, 244, 2, Theme::TEXT, Theme::BG, MC_DATUM);
    revision_ = state.radioRevision;
}
