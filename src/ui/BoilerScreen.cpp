#include "ui/BoilerScreen.h"
#include "display/Theme.h"

void BoilerScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void BoilerScreen::update(DisplayDriver& display, const AppState& state) {
    if (revision_ == state.boilerRevision) return;
    display.tft().fillRect(0, 0, 240, 266, Theme::BG);
    drawContent(display, state);
}

void BoilerScreen::drawContent(DisplayDriver& display, const AppState& state) {

    display.drawUtf8("KOCIOŁ", 120, 28, 4,
                     Theme::ACCENT, Theme::BG, MC_DATUM);

    String cur = isnan(state.boilerCurrentTemp) ? "--.- C" : String(state.boilerCurrentTemp, 1) + " C";
    String target = isnan(state.boilerTargetTemp) ? "--.- C" : String(state.boilerTargetTemp, 1) + " C";

    display.drawUtf8("Aktualna", 120, 82, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(cur, 120, 116, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8("Zadana: " + target, 120, 162, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    display.drawUtf8(state.boilerEnabled ? "PRACA" : "STOP", 120, 210, 4,
                     Theme::TEXT, Theme::BG, MC_DATUM);
    revision_ = state.boilerRevision;
}
