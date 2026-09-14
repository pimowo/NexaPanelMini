#include "core/Navigation.h"

void Navigation::begin() {
    current_ = ScreenId::HOME;
}

void Navigation::goTo(ScreenId screen) {
    current_ = screen;
}

ScreenId Navigation::currentScreen() const {
    return current_;
}

MainSection Navigation::activeSection() const {
    switch (current_) {
        case ScreenId::RADIO:
            return MainSection::RADIO;
        case ScreenId::BOILER:
            return MainSection::BOILER;
        case ScreenId::WEATHER_DETAILS:
        case ScreenId::BOILER_TEMPERATURE:
            return MainSection::NONE;
        case ScreenId::HOME:
        default:
            return MainSection::HOME;
    }
}
