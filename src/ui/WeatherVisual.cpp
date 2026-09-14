#include "ui/WeatherVisual.h"

namespace {
struct WmoRange {
    uint8_t first;
    uint8_t last;
    WeatherIconKind icon;
};

const WmoRange WMO_MAP[] = {
    {0, 0, WeatherIconKind::CLEAR},
    {1, 1, WeatherIconKind::MAINLY_CLEAR},
    {2, 2, WeatherIconKind::PARTLY_CLOUDY},
    {3, 3, WeatherIconKind::CLOUDY},
    {45, 48, WeatherIconKind::FOG},
    {51, 55, WeatherIconKind::DRIZZLE},
    {56, 57, WeatherIconKind::FREEZING_RAIN},
    {61, 63, WeatherIconKind::RAIN},
    {65, 65, WeatherIconKind::HEAVY_RAIN},
    {66, 67, WeatherIconKind::FREEZING_RAIN},
    {71, 77, WeatherIconKind::SNOW},
    {80, 81, WeatherIconKind::RAIN},
    {82, 82, WeatherIconKind::HEAVY_RAIN},
    {85, 86, WeatherIconKind::SNOW},
    {95, 95, WeatherIconKind::THUNDERSTORM},
    {96, 99, WeatherIconKind::THUNDERSTORM_HAIL}
};

constexpr uint16_t SUN = TFT_YELLOW;
constexpr uint16_t CLOUD = 0xC618;
constexpr uint16_t CLOUD_DARK = 0x8410;
constexpr uint16_t RAIN = 0x04FF;
constexpr uint16_t ICE = 0xBFFF;
constexpr uint16_t LIGHTNING = 0xFFE0;

void drawSun(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius) {
    tft.fillCircle(cx, cy, radius, SUN);
    const int16_t ray = radius + 4;
    tft.drawFastHLine(cx - ray - 3, cy, 4, SUN);
    tft.drawFastHLine(cx + ray, cy, 4, SUN);
    tft.drawFastVLine(cx, cy - ray - 3, 4, SUN);
    tft.drawFastVLine(cx, cy + ray, 4, SUN);
    tft.drawLine(cx - ray, cy - ray, cx - ray + 3, cy - ray + 3, SUN);
    tft.drawLine(cx + ray, cy - ray, cx + ray - 3, cy - ray + 3, SUN);
    tft.drawLine(cx - ray, cy + ray, cx - ray + 3, cy + ray - 3, SUN);
    tft.drawLine(cx + ray, cy + ray, cx + ray - 3, cy + ray - 3, SUN);
}

void drawCloud(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
               uint16_t color = CLOUD) {
    const int16_t baseY = y + size * 5 / 8;
    tft.fillCircle(x + size * 3 / 10, baseY, size / 5, color);
    tft.fillCircle(x + size / 2, y + size * 2 / 5, size / 4, color);
    tft.fillCircle(x + size * 7 / 10, baseY, size / 5, color);
    tft.fillRoundRect(x + size / 7, baseY, size * 5 / 7,
                      size / 4, size / 10, color);
}

void drawRain(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
              bool heavy) {
    const int16_t startY = y + size * 3 / 4;
    const int16_t length = heavy ? size / 4 : size / 6;
    for (uint8_t index = 0; index < 3; ++index) {
        const int16_t startX = x + size / 4 + index * size / 4;
        tft.drawLine(startX, startY, startX - 2, startY + length, RAIN);
        if (heavy) {
            tft.drawLine(startX + 1, startY, startX - 1,
                         startY + length, RAIN);
        }
    }
}

void drawSnow(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
              uint16_t color = ICE) {
    const int16_t cy = y + size * 7 / 8;
    for (uint8_t index = 0; index < 3; ++index) {
        const int16_t cx = x + size / 4 + index * size / 4;
        tft.drawFastHLine(cx - 2, cy, 5, color);
        tft.drawFastVLine(cx, cy - 2, 5, color);
    }
}
}

WeatherIconKind weatherIconForCode(int code) {
    for (const WmoRange& range : WMO_MAP) {
        if (code >= range.first && code <= range.last) return range.icon;
    }
    return WeatherIconKind::CLOUDY;
}

const char* weatherDescriptionPl(int code) {
    switch (weatherIconForCode(code)) {
        case WeatherIconKind::CLEAR: return "Bezchmurnie";
        case WeatherIconKind::MAINLY_CLEAR: return "Pogodnie";
        case WeatherIconKind::PARTLY_CLOUDY: return "Częściowe zachmurzenie";
        case WeatherIconKind::CLOUDY: return "Pochmurno";
        case WeatherIconKind::FOG: return "Mgła";
        case WeatherIconKind::DRIZZLE: return "Mżawka";
        case WeatherIconKind::FREEZING_RAIN: return "Marznący deszcz";
        case WeatherIconKind::RAIN: return "Deszcz";
        case WeatherIconKind::HEAVY_RAIN: return "Ulewa";
        case WeatherIconKind::SNOW: return "Śnieg";
        case WeatherIconKind::THUNDERSTORM: return "Burza";
        case WeatherIconKind::THUNDERSTORM_HAIL: return "Burza z gradem";
    }
    return "Pochmurno";
}

void drawWeatherIcon(TFT_eSPI& tft, int16_t x, int16_t y,
                     int16_t size, int code) {
    const WeatherIconKind icon = weatherIconForCode(code);
    if (icon == WeatherIconKind::CLEAR) {
        drawSun(tft, x + size / 2, y + size / 2, size / 5);
        return;
    }
    if (icon == WeatherIconKind::MAINLY_CLEAR ||
        icon == WeatherIconKind::PARTLY_CLOUDY) {
        drawSun(tft, x + size * 2 / 5, y + size * 2 / 5, size / 6);
        drawCloud(tft, x + size / 8, y + size / 5, size * 7 / 8);
        return;
    }
    if (icon == WeatherIconKind::FOG) {
        drawCloud(tft, x, y, size, CLOUD_DARK);
        for (uint8_t index = 0; index < 3; ++index) {
            tft.drawFastHLine(x + size / 8, y + size * 3 / 4 + index * 4,
                              size * 3 / 4, CLOUD);
        }
        return;
    }
    drawCloud(tft, x, y, size,
              icon == WeatherIconKind::THUNDERSTORM ||
              icon == WeatherIconKind::THUNDERSTORM_HAIL
                  ? CLOUD_DARK : CLOUD);
    if (icon == WeatherIconKind::DRIZZLE) {
        for (uint8_t index = 0; index < 3; ++index) {
            tft.fillCircle(x + size / 4 + index * size / 4,
                           y + size * 7 / 8, 1, RAIN);
        }
    } else if (icon == WeatherIconKind::RAIN) {
        drawRain(tft, x, y, size, false);
    } else if (icon == WeatherIconKind::HEAVY_RAIN) {
        drawRain(tft, x, y, size, true);
    } else if (icon == WeatherIconKind::FREEZING_RAIN) {
        drawRain(tft, x, y, size, false);
        drawSnow(tft, x, y, size, ICE);
    } else if (icon == WeatherIconKind::SNOW) {
        drawSnow(tft, x, y, size);
    } else if (icon == WeatherIconKind::THUNDERSTORM ||
               icon == WeatherIconKind::THUNDERSTORM_HAIL) {
        const int16_t cx = x + size / 2;
        const int16_t top = y + size * 2 / 3;
        tft.fillTriangle(cx, top, cx - size / 8, top + size / 5,
                         cx, top + size / 5, LIGHTNING);
        tft.fillTriangle(cx, top + size / 7, cx + size / 8,
                         top + size / 7, cx - size / 10,
                         y + size - 1, LIGHTNING);
        if (icon == WeatherIconKind::THUNDERSTORM_HAIL) {
            tft.fillCircle(x + size / 4, y + size * 7 / 8, 2, ICE);
            tft.fillCircle(x + size * 3 / 4, y + size * 7 / 8, 2, ICE);
        }
    }
}