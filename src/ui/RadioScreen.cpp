#include "ui/RadioScreen.h"
#include "ui/BottomBar.h"
#include "display/Theme.h"

namespace {
constexpr int16_t ICON_SIZE = 20;
constexpr int16_t TOP_BUTTON_Y = 6;
constexpr int16_t TOP_BUTTON_HEIGHT = 44;
constexpr int16_t PLAYBACK_Y = 174;
constexpr int16_t PLAYBACK_HEIGHT = 54;
constexpr int16_t TEXT_X = 10;
constexpr int16_t TEXT_WIDTH = 220;

const uint8_t SPEAKER_ICON[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x20,0x00, 0x00,0x30,0x00,
    0x00,0x3C,0x00, 0x00,0x3E,0x00, 0x80,0x3F,0x00, 0xFC,0x3F,0x00,
    0xFC,0x3F,0x00, 0xFC,0x3F,0x00, 0xFC,0x3F,0x00, 0xFC,0x3F,0x00,
    0xFC,0x3F,0x00, 0xFC,0x3F,0x00, 0x80,0x3F,0x00, 0x00,0x3E,0x00,
    0x00,0x3C,0x00, 0x00,0x30,0x00, 0x00,0x20,0x00, 0x00,0x00,0x00
};
const uint8_t PREVIOUS_ICON[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x1C,0x00,0x02,
    0x1C,0x80,0x03, 0x1C,0xC0,0x03, 0x1C,0xF0,0x03, 0x1C,0xF8,0x03,
    0x1C,0xFE,0x03, 0x1C,0xFF,0x03, 0xDC,0xFF,0x03, 0x1C,0xFF,0x03,
    0x1C,0xFE,0x03, 0x1C,0xF8,0x03, 0x1C,0xF0,0x03, 0x1C,0xC0,0x03,
    0x1C,0x80,0x03, 0x1C,0x00,0x02, 0x00,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t PLAY_ICON[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x10,0x00,0x00, 0x70,0x00,0x00,
    0xF0,0x00,0x00, 0xF0,0x03,0x00, 0xF0,0x07,0x00, 0xF0,0x1F,0x00,
    0xF0,0x7F,0x00, 0xF0,0xFF,0x00, 0xF0,0xFF,0x03, 0xF0,0xFF,0x00,
    0xF0,0x7F,0x00, 0xF0,0x1F,0x00, 0xF0,0x07,0x00, 0xF0,0x03,0x00,
    0xF0,0x00,0x00, 0x70,0x00,0x00, 0x10,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t PAUSE_ICON[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03,
    0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03,
    0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03,
    0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03,
    0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0xF0,0xE1,0x03, 0x00,0x00,0x00
};
const uint8_t NEXT_ICON[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x08,0x00,0x07,
    0x38,0x00,0x07, 0x78,0x00,0x07, 0xF8,0x01,0x07, 0xF8,0x03,0x07,
    0xF8,0x0F,0x07, 0xF8,0x1F,0x07, 0xF8,0x7F,0x07, 0xF8,0x1F,0x07,
    0xF8,0x0F,0x07, 0xF8,0x03,0x07, 0xF8,0x01,0x07, 0x78,0x00,0x07,
    0x38,0x00,0x07, 0x08,0x00,0x07, 0x00,0x00,0x00, 0x00,0x00,0x00
};
}

void RadioScreen::draw(DisplayDriver& display, const AppState& state) {
    auto& tft = display.tft();
    tft.fillRect(0, 0, 240, 266, Theme::BG);
    drawStatic(display);
    drawStation(display, state, false);
    drawArtist(display, state, false);
    drawTitle(display, state, false);
    drawPlayback(display, state, false);
    drawVolume(display, state, false);
    cacheState(state);
}

void RadioScreen::update(DisplayDriver& display, const AppState& state) {
    if (revision_ == state.radioRevision) return;
    if (!cacheValid_ || station_ != state.radioStation) {
        drawStation(display, state);
    }
    if (!cacheValid_ || artist_ != state.radioArtist) {
        drawArtist(display, state);
    }
    if (!cacheValid_ || title_ != state.radioTitle) {
        drawTitle(display, state);
    }
    if (!cacheValid_ || playing_ != state.radioPlaying) {
        drawPlayback(display, state);
    }
    if (!cacheValid_ || volume_ != state.radioVolume ||
        offlineError_ != state.radioOfflineError) {
        drawVolume(display, state);
    }
    cacheState(state);
}

void RadioScreen::drawStatic(DisplayDriver& display) {
    auto& tft = display.tft();
    tft.drawRect(4, TOP_BUTTON_Y, 56, TOP_BUTTON_HEIGHT, Theme::DIM);
    tft.drawRect(180, TOP_BUTTON_Y, 56, TOP_BUTTON_HEIGHT, Theme::DIM);
    tft.drawXBitmap(12, 18, SPEAKER_ICON, ICON_SIZE, ICON_SIZE, Theme::TEXT);
    tft.drawXBitmap(188, 18, SPEAKER_ICON, ICON_SIZE, ICON_SIZE, Theme::TEXT);
    tft.drawFastHLine(37, 28, 12, Theme::TEXT);
    tft.drawFastHLine(213, 28, 12, Theme::TEXT);
    tft.drawFastVLine(219, 22, 13, Theme::TEXT);

    for (int16_t x = 0; x < 240; x += 80) {
        tft.drawRect(x + 4, PLAYBACK_Y, 72, PLAYBACK_HEIGHT, Theme::DIM);
    }
    tft.drawXBitmap(30, 191, PREVIOUS_ICON, ICON_SIZE, ICON_SIZE, Theme::TEXT);
    tft.drawXBitmap(190, 191, NEXT_ICON, ICON_SIZE, ICON_SIZE, Theme::TEXT);
}

void RadioScreen::drawStation(DisplayDriver& display, const AppState& state,
                              bool clearRegion) {
    drawClippedText(display, state.radioStation, 76, clearRegion);
}

void RadioScreen::drawArtist(DisplayDriver& display, const AppState& state,
                             bool clearRegion) {
    drawClippedText(display, state.radioArtist, 108, clearRegion);
}

void RadioScreen::drawTitle(DisplayDriver& display, const AppState& state,
                            bool clearRegion) {
    drawClippedText(display, state.radioTitle, 138, clearRegion);
}

void RadioScreen::drawPlayback(DisplayDriver& display, const AppState& state,
                               bool clearRegion) {
    auto& tft = display.tft();
    if (clearRegion) {
        tft.fillRect(84, PLAYBACK_Y, 72, PLAYBACK_HEIGHT, Theme::BG);
        tft.drawRect(84, PLAYBACK_Y, 72, PLAYBACK_HEIGHT, Theme::DIM);
    }
    tft.drawXBitmap(110, 191,
                    state.radioPlaying ? PAUSE_ICON : PLAY_ICON,
                    ICON_SIZE, ICON_SIZE, Theme::TEXT);
}

void RadioScreen::drawVolume(DisplayDriver& display, const AppState& state,
                             bool clearRegion) {
    if (clearRegion) display.tft().fillRect(62, 6, 116, 44, Theme::BG);
    const String value = state.radioOfflineError
        ? "Błąd"
        : "VOL " + String(state.radioVolume);
    display.drawUtf8(value, 120, 28, 4,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
}

void RadioScreen::drawClippedText(DisplayDriver& display, const String& text,
                                  int16_t y, bool clearRegion) {
    auto& tft = display.tft();
    tft.setViewport(TEXT_X, y - 15, TEXT_WIDTH, 30, true);
    if (clearRegion) tft.fillRect(0, 0, TEXT_WIDTH, 30, Theme::BG);
    tft.setTextWrap(false, false);
    display.drawUtf8(text, 0, 15, 2,
                     Theme::TEXT, Theme::BG, ML_DATUM);
    tft.setTextWrap(true, true);
    tft.resetViewport();
}

void RadioScreen::cacheState(const AppState& state) {
    station_ = state.radioStation;
    artist_ = state.radioArtist;
    title_ = state.radioTitle;
    volume_ = state.radioVolume;
    offlineError_ = state.radioOfflineError;
    playing_ = state.radioPlaying;
    revision_ = state.radioRevision;
    cacheValid_ = true;
}

RadioAction RadioScreen::actionAt(const TouchPoint& point) const {
    if (!point.touched || point.y < 0 || point.y >= BottomBar::Y) {
        return RadioAction::NONE;
    }
    if (point.y < 58) {
        if (point.x < 64) return RadioAction::VOLUME_DOWN;
        if (point.x >= 176) return RadioAction::VOLUME_UP;
    }
    if (point.y >= 168 && point.y < 232) {
        if (point.x < 80) return RadioAction::PREVIOUS;
        if (point.x < 160) return RadioAction::TOGGLE_PLAY;
        return RadioAction::NEXT;
    }
    return RadioAction::NONE;
}
