#pragma once
#include <Arduino.h>

enum class ScreenId : uint8_t {
    HOME,
    RADIO,
    BOILER,
    WEATHER_DETAILS
};

enum class MainSection : uint8_t {
    NONE,
    RADIO,
    HOME,
    BOILER
};

class Navigation {
public:
    void begin();
    void goTo(ScreenId screen);

    ScreenId currentScreen() const;
    MainSection activeSection() const;

private:
    ScreenId current_ = ScreenId::HOME;
};
