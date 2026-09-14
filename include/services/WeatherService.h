#pragma once
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "core/AppState.h"

class WeatherService {
public:
    void begin();
    void update(AppState& state);

private:
    bool fetch(AppState& state);
    bool applyResponse(Stream& stream, AppState& state);
    String buildUrl() const;

    BearSSL::WiFiClientSecure client_;
    uint32_t nextAttemptMs_ = 0;
    bool requestInProgress_ = false;
};
