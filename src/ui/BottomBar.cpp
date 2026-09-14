#include "ui/BottomBar.h"
#include "display/Theme.h"

namespace {
constexpr int16_t HOME_ICON_SIZE = 24;

const uint8_t HOME_ICON[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0C, 0x00, 0x00, 0x7E, 0x0E, 0x00, 0xFF, 0x0E,
    0x80, 0xFF, 0x0F, 0xC0, 0xFF, 0x0F, 0xE0, 0xFF, 0x07,
    0xF0, 0xFF, 0x0F, 0xF8, 0xFF, 0x1F, 0xFC, 0xFF, 0x3F,
    0xFC, 0xFF, 0x3F, 0xFC, 0xFF, 0x3F, 0xFC, 0xFF, 0x3F,
    0xFC, 0xC3, 0x3F, 0xFC, 0xC3, 0x3F, 0xFC, 0xC3, 0x3F,
    0xFC, 0xC3, 0x3F, 0xFC, 0xC3, 0x3F, 0xFC, 0xC3, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
}

void BottomBar::update(DisplayDriver& display, MainSection active) {
    if (rendered_ && active == lastActiveSection_) return;

    auto& tft = display.tft();
    const bool firstRender = !rendered_;

    if (firstRender) {
        tft.drawFastHLine(0, Y, 240, Theme::DIM);
        drawSegment(display, MainSection::BOILER,
                    active == MainSection::BOILER);
        drawSegment(display, MainSection::HOME,
                    active == MainSection::HOME);
        drawSegment(display, MainSection::RADIO,
                    active == MainSection::RADIO);
    } else {
        if (lastActiveSection_ != MainSection::NONE) {
            drawSegment(display, lastActiveSection_, false);
        }
        if (active != MainSection::NONE) {
            drawSegment(display, active, true);
        }
    }

    rendered_ = true;
    lastActiveSection_ = active;
}

void BottomBar::invalidate() {
    rendered_ = false;
    lastActiveSection_ = MainSection::NONE;
}

void BottomBar::drawSegment(DisplayDriver& display, MainSection section,
                            bool active) {
    auto& tft = display.tft();
    constexpr int16_t width = 80;
    const int16_t x = segmentX(section);
    const uint16_t background = active ? Theme::ACCENT : Theme::BG;
    const uint16_t foreground = active ? Theme::BG : Theme::TEXT;

    tft.fillRect(x, Y + 1, width, HEIGHT - 1, background);
    if (section == MainSection::HOME) {
        const int16_t iconX = x + (width - HOME_ICON_SIZE) / 2;
        const int16_t iconY = Y + 1 + (HEIGHT - 1 - HOME_ICON_SIZE) / 2;
        tft.drawXBitmap(iconX, iconY, HOME_ICON,
                        HOME_ICON_SIZE, HOME_ICON_SIZE, foreground);
        return;
    }

    const char* label = section == MainSection::BOILER ? "KOCIOŁ" : "RADIO";
    display.drawUtf8(label, x + width / 2, Y + HEIGHT / 2,
                     2, foreground, background, MC_DATUM);
}

int16_t BottomBar::segmentX(MainSection section) const {
    switch (section) {
        case MainSection::NONE:
            return -80;
        case MainSection::BOILER:
            return 0;
        case MainSection::HOME:
            return 80;
        case MainSection::RADIO:
            return 160;
    }
    return 0;
}

bool BottomBar::handleTouch(const TouchPoint& point, Navigation& navigation) {
    MainSection section;
    if (!sectionAt(point, section)) return false;

    switch (section) {
        case MainSection::NONE:
            return false;
        case MainSection::BOILER:
            navigation.goTo(ScreenId::BOILER);
            break;
        case MainSection::HOME:
            navigation.goTo(ScreenId::HOME);
            break;
        case MainSection::RADIO:
            navigation.goTo(ScreenId::RADIO);
            break;
    }
    return true;
}

bool BottomBar::sectionAt(const TouchPoint& point, MainSection& section) const {
    if (!point.touched || point.y < Y) return false;
    if (hitBoiler(point.x, point.y)) section = MainSection::BOILER;
    else if (hitHome(point.x, point.y)) section = MainSection::HOME;
    else if (hitRadio(point.x, point.y)) section = MainSection::RADIO;
    else return false;
    return true;
}

bool BottomBar::hitBoiler(int16_t x, int16_t y) const{return y >= Y && x >= 0   && x < 80; }
bool BottomBar::hitHome(int16_t x, int16_t y) const  { return y >= Y && x >= 80  && x < 160; }
bool BottomBar::hitRadio(int16_t x, int16_t y) const { return y >= Y && x >= 160 && x < 240; }
