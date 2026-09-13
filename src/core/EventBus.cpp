#include "core/EventBus.h"

void EventBus::publish(AppEvent event) {
    pending_ = event;
}

AppEvent EventBus::consume() {
    AppEvent out = pending_;
    pending_ = AppEvent::NONE;
    return out;
}
