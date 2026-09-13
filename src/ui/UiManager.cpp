#include "ui/UiManager.h"

void UiManager::begin(DisplayDriver& display, TouchDriver& touch, Navigation& navigation) {
    display_ = &display;
    touch_ = &touch;
    navigation_ = &navigation;
    lastScreen_ = navigation_->currentScreen();
    redrawPending_ = true;
}

void UiManager::update(AppState& state) {
    state_ = &state;

    TouchPoint p = touch_->read();
    if (p.touched && !touchDown_) {
        touchDown_ = true;
        handleTouch(p);
    } else if (!p.touched) {
        touchDown_ = false;
    }

    if (navigation_->currentScreen() != lastScreen_) {
        lastScreen_ = navigation_->currentScreen();
        redrawPending_ = true;
    }

    if (redrawPending_) {
        redrawPending_ = false;
        redraw();
    } else {
        switch (navigation_->currentScreen()) {
            case ScreenId::HOME:
                home_.update(*display_, state);
                break;
            case ScreenId::RADIO:
                radio_.update(*display_, state);
                break;
            case ScreenId::BOILER:
                boiler_.update(*display_, state);
                break;
            case ScreenId::WEATHER_DETAILS:
                weather_.update(*display_, state);
                break;
        }
    }

    bottomBar_.update(*display_, navigation_->activeSection());
}

void UiManager::handleTouch(const TouchPoint& point) {
    MainSection section;
    if (bottomBar_.sectionAt(point, section)) {
        bottomBar_.handleTouch(point, *navigation_);
        return;
    }

    if (navigation_->currentScreen() == ScreenId::HOME) {
        if (point.y >= 130 && point.y < BottomBar::Y) {
            navigation_->goTo(ScreenId::WEATHER_DETAILS);
            redrawPending_ = true;
        }
    }
}

void UiManager::redraw() {
    if (!display_ || !state_ || !navigation_) return;

    switch (navigation_->currentScreen()) {
        case ScreenId::HOME:
            home_.draw(*display_, *state_);
            break;
        case ScreenId::RADIO:
            radio_.draw(*display_, *state_);
            break;
        case ScreenId::BOILER:
            boiler_.draw(*display_, *state_);
            break;
        case ScreenId::WEATHER_DETAILS:
            weather_.draw(*display_, *state_);
            break;
    }

}
