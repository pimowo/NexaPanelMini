#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

enum class WeatherIconKind : uint8_t {
    CLEAR,
    MAINLY_CLEAR,
    PARTLY_CLOUDY,
    CLOUDY,
    FOG,
    DRIZZLE,
    FREEZING_RAIN,
    RAIN,
    HEAVY_RAIN,
    SNOW,
    THUNDERSTORM,
    THUNDERSTORM_HAIL
};

WeatherIconKind weatherIconForCode(int code);
const char* weatherDescriptionPl(int code);
void drawWeatherIcon(TFT_eSPI& tft, int16_t x, int16_t y,
                     int16_t size, int code);