#pragma once

enum class AppEvent {
    NONE,
    WIFI_CHANGED,
    TIME_UPDATED,
    WEATHER_UPDATED,
    RADIO_UPDATED,
    BOILER_UPDATED,
    NAVIGATION_CHANGED
};

class EventBus {
public:
    void publish(AppEvent event);
    AppEvent consume();

private:
    AppEvent pending_ = AppEvent::NONE;
};
