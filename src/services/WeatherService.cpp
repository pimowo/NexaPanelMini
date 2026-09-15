#include "services/WeatherService.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include "config.h"

namespace {
const char* weekdayName(const char* isoDate) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (sscanf(isoDate, "%d-%d-%d", &year, &month, &day) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31) {
        return "";
    }
    static const uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    static const char* weekdays[] = {
        "Niedziela", "Poniedziałek", "Wtorek", "Środa", "Czwartek",
        "Piątek", "Sobota"
    };
    if (month < 3) --year;
    const int weekday = (year + year / 4 - year / 100 + year / 400 +
                         offsets[month - 1] + day) % 7;
    return weekdays[weekday];
}
}

void WeatherService::begin() {
    client_.setInsecure();
    client_.setBufferSizes(512, 512);
    nextAttemptMs_ = 0;
}

void WeatherService::update(AppState& state) {
    if (requestInProgress_ || WiFi.status() != WL_CONNECTED ||
        AppConfig::WEATHER_LATITUDE[0] == '\0' ||
        AppConfig::WEATHER_LONGITUDE[0] == '\0') {
        return;
    }

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextAttemptMs_) < 0) return;

    requestInProgress_ = true;
    const uint32_t startedMs = millis();
    const bool success = fetch(state);
    requestInProgress_ = false;
    nextAttemptMs_ = millis() +
        (success ? AppConfig::WEATHER_REFRESH_MS : AppConfig::WEATHER_RETRY_MS);
    Serial.printf("WEATHER %s: %lu ms\n", success ? "UPDATED" : "ERROR",
                  static_cast<unsigned long>(millis() - startedMs));
}

bool WeatherService::fetch(AppState& state) {
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(5000);
    http.setReuse(false);
    if (!http.begin(client_, buildUrl())) {
        Serial.println("WEATHER HTTP begin failed");
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("WEATHER HTTP status=%d\n", status);
        http.end();
        return false;
    }
    const bool success = applyResponse(http.getStream(), state);
    http.end();
    return success;
}

bool WeatherService::applyResponse(Stream& stream, AppState& state) {
    DynamicJsonDocument document(2048);
    const DeserializationError error = deserializeJson(document, stream);
    if (error) {
        Serial.printf("WEATHER JSON error=%s\n", error.c_str());
        return false;
    }

    const JsonObjectConst current = document["current"];
    const JsonObjectConst daily = document["daily"];
    const JsonArrayConst times = daily["time"];
    const JsonArrayConst codes = daily["weather_code"];
    const JsonArrayConst maximums = daily["temperature_2m_max"];
    const JsonArrayConst minimums = daily["temperature_2m_min"];
    if (current.isNull() || times.size() < 4 || codes.size() < 4 ||
        maximums.size() < 4 || minimums.size() < 4) {
        Serial.printf("WEATHER JSON fields current=%u time=%u code=%u "
                      "max=%u min=%u\n",
                      current.isNull() ? 0U : 1U,
                      static_cast<unsigned>(times.size()),
                      static_cast<unsigned>(codes.size()),
                      static_cast<unsigned>(maximums.size()),
                      static_cast<unsigned>(minimums.size()));
        return false;
    }

    const float currentTemperature = current["temperature_2m"] | NAN;
    const float currentPressure = current["pressure_msl"] | NAN;
    const int currentCode = current["weather_code"] | -1;
    if (!isfinite(currentTemperature) || currentCode < 0) {
        Serial.println("WEATHER JSON invalid current data");
        return false;
    }

    WeatherDay parsed[4];
    for (uint8_t index = 0; index < 4; ++index) {
        const char* isoDate = times[index] | "";
        const float maximum = maximums[index] | NAN;
        const float minimum = minimums[index] | NAN;
        const int code = codes[index] | -1;
        if (strlen(isoDate) != 10 || !isfinite(maximum) ||
            !isfinite(minimum) || code < 0) {
            Serial.printf("WEATHER JSON invalid day=%u\n", index);
            return false;
        }
        parsed[index].dateText = String(isoDate + 8) + "." +
                 String(isoDate + 5).substring(0, 2) + " " +
                 weekdayName(isoDate);
        parsed[index].weatherCode = code;
        parsed[index].tempMax = maximum;
        parsed[index].tempMin = minimum;
    }

    bool changed = !state.weatherValid ||
                   state.currentWeatherCode != currentCode ||
                   fabsf(state.outsideTemp - currentTemperature) >= 0.05F ||
                   (isfinite(currentPressure)
                        ? (!isfinite(state.outsidePressure) ||
                           fabsf(state.outsidePressure - currentPressure) >=
                               0.5F)
                        : isfinite(state.outsidePressure));
    state.weatherValid = true;
    state.currentWeatherCode = currentCode;
    state.outsideTemp = currentTemperature;
    state.outsidePressure = isfinite(currentPressure) ? currentPressure : NAN;
    state.todayMax = parsed[0].tempMax;
    state.todayMin = parsed[0].tempMin;
    for (uint8_t index = 0; index < 4; ++index) {
        const WeatherDay& old = state.forecast[index];
        changed |= old.dateText != parsed[index].dateText ||
                   old.weatherCode != parsed[index].weatherCode ||
                   isnan(old.tempMax) || isnan(old.tempMin) ||
                   fabsf(old.tempMax - parsed[index].tempMax) >= 0.05F ||
                   fabsf(old.tempMin - parsed[index].tempMin) >= 0.05F;
        state.forecast[index] = parsed[index];
    }
    if (changed) ++state.weatherRevision;
    return true;
}

String WeatherService::buildUrl() const {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=";
    url += AppConfig::WEATHER_LATITUDE;
    url += "&longitude=";
    url += AppConfig::WEATHER_LONGITUDE;
    url += "&current=temperature_2m,pressure_msl,weather_code";
    url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
    url += "&timezone=Europe%2FWarsaw&forecast_days=4";
    return url;
}
