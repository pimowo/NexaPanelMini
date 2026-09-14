#include "ui/UiManager.h"
#include "config.h"

void UiManager::begin(DisplayDriver& display, TouchDriver& touch,
                      Navigation& navigation, RadioService& radioService,
                      BoilerService& boilerService) {
    display_ = &display;
    touch_ = &touch;
    navigation_ = &navigation;
    radioService_ = &radioService;
    boilerService_ = &boilerService;
    lastScreen_ = navigation_->currentScreen();
    redrawPending_ = true;
    lastTouchMs_ = millis();
}

void UiManager::update(AppState& state) {
    state_ = &state;

#ifdef UI_DRAW_DIAGNOSTICS
    static uint32_t nextDiagnosticScreenMs = 2000;
    static uint8_t diagnosticScreen = 0;
    if (diagnosticScreen < 3 && millis() >= nextDiagnosticScreenMs) {
        const ScreenId screens[] = {
            ScreenId::RADIO, ScreenId::BOILER, ScreenId::WEATHER_DETAILS
        };
        navigation_->goTo(screens[diagnosticScreen++]);
        nextDiagnosticScreenMs += 2000;
    }
#endif

    TouchPoint p = touch_->read();
    if (p.touched && !touchDown_) {
        lastTouchMs_ = millis();
        touchDown_ = true;
        handleTouch(p);
        if (navigation_->currentScreen() == ScreenId::RADIO) {
            const RadioAction action = radio_.actionAt(p);
            if (action == RadioAction::VOLUME_DOWN ||
                action == RadioAction::VOLUME_UP) {
                heldRadioAction_ = action;
                nextVolumeRepeatMs_ = millis() +
                                     AppConfig::UI_VOLUME_HOLD_INITIAL_MS;
            }
        }
    } else if (p.touched) {
        updateVolumeHold(p);
    } else if (!p.touched) {
        touchDown_ = false;
        heldRadioAction_ = RadioAction::NONE;
    }

    if (navigation_->currentScreen() != lastScreen_) {
        lastScreen_ = navigation_->currentScreen();
        redrawPending_ = true;
    }

    const uint32_t now = millis();
    if (navigation_->currentScreen() != ScreenId::HOME &&
        static_cast<int32_t>(now - lastTouchMs_) >=
            static_cast<int32_t>(AppConfig::UI_HOME_TIMEOUT_MS)) {
        navigation_->goTo(ScreenId::HOME);
        heldRadioAction_ = RadioAction::NONE;
        touchDown_ = false;
        lastScreen_ = ScreenId::HOME;
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
        return;
    }

    if (navigation_->currentScreen() == ScreenId::RADIO && radioService_) {
        switch (radio_.actionAt(point)) {
            case RadioAction::VOLUME_DOWN:
                radioService_->volumeDown();
                break;
            case RadioAction::VOLUME_UP:
                radioService_->volumeUp();
                break;
            case RadioAction::PREVIOUS:
                radioService_->previous();
                break;
            case RadioAction::TOGGLE_PLAY:
                radioService_->togglePlay();
                break;
            case RadioAction::NEXT:
                radioService_->next();
                break;
            case RadioAction::NONE:
                break;
        }
        return;
    }

    if (navigation_->currentScreen() == ScreenId::BOILER && boilerService_ &&
        state_) {
        switch (boiler_.actionAt(point)) {
            case BoilerAction::TOGGLE_POWER:
                boilerService_->setPower(!state_->boilerPowerOn);
                break;
            case BoilerAction::TOGGLE_HVAC:
                boilerService_->setHvacEnabled(!state_->boilerHvacHeat);
                break;
            case BoilerAction::COMFORT:
                boilerService_->setComfortMode(true);
                break;
            case BoilerAction::SLEEP:
                boilerService_->setComfortMode(false);
                break;
            case BoilerAction::NONE:
                break;
        }
    }
}

void UiManager::updateVolumeHold(const TouchPoint& point) {
    if (heldRadioAction_ == RadioAction::NONE || !radioService_ ||
        navigation_->currentScreen() != ScreenId::RADIO ||
        radio_.actionAt(point) != heldRadioAction_) {
        heldRadioAction_ = RadioAction::NONE;
        return;
    }
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextVolumeRepeatMs_) < 0) return;
    if (heldRadioAction_ == RadioAction::VOLUME_DOWN) radioService_->volumeDown();
    else radioService_->volumeUp();
    nextVolumeRepeatMs_ = now + AppConfig::UI_VOLUME_HOLD_REPEAT_MS;
}

void UiManager::redraw() {
    if (!display_ || !state_ || !navigation_) return;

#ifdef UI_DRAW_DIAGNOSTICS
    display_->resetDrawDiagnostics();
    const uint32_t drawStartedUs = micros();
    const char* screenName = "UNKNOWN";
#endif
    switch (navigation_->currentScreen()) {
        case ScreenId::HOME:
#ifdef UI_DRAW_DIAGNOSTICS
            screenName = "HOME";
#endif
            home_.draw(*display_, *state_);
            break;
        case ScreenId::RADIO:
#ifdef UI_DRAW_DIAGNOSTICS
            screenName = "RADIO";
#endif
            radio_.draw(*display_, *state_);
            break;
        case ScreenId::BOILER:
#ifdef UI_DRAW_DIAGNOSTICS
            screenName = "BOILER";
#endif
            boiler_.draw(*display_, *state_);
            break;
        case ScreenId::WEATHER_DETAILS:
#ifdef UI_DRAW_DIAGNOSTICS
            screenName = "WEATHER_DETAILS";
#endif
            weather_.draw(*display_, *state_);
            break;
    }

#ifdef UI_DRAW_DIAGNOSTICS
    const uint32_t elapsedUs = micros() - drawStartedUs;
    Serial.printf("UI DRAW %s = %lu us (%lu ms)\n", screenName,
                  static_cast<unsigned long>(elapsedUs),
                  static_cast<unsigned long>((elapsedUs + 500U) / 1000U));
    display_->printDrawDiagnostics(elapsedUs);
#endif
}
