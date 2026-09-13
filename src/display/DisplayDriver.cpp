#include "display/DisplayDriver.h"
#include "display/NotoSansPl.h"
#include "pins.h"

void DisplayDriver::begin() {
    pinMode(PIN_TOUCH_CS, OUTPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    tft_.init();
    tft_.setRotation(0); // pion 240x320
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.setTextSize(2);
    tft_.setCursor(10, 10);
    tft_.print("NEXA TEST");
    tft_.setTextSize(1);

    Serial.printf("DISPLAY ILI9341: %dx%d, rotation=0\n",
                  tft_.width(), tft_.height());
}

TFT_eSPI& DisplayDriver::tft() {
    return tft_;
}

int16_t DisplayDriver::drawUtf8(const char* text, int16_t x, int16_t y,
                                uint8_t font, uint16_t foreground,
                                uint16_t background, uint8_t datum) {
    if (text == nullptr) return 0;

    selectFont(font);
    tft_.setTextColor(foreground, background);
    tft_.setTextDatum(datum);
    return loadedSmoothFont_ == 0
               ? tft_.drawString(text, x, y, font)
               : tft_.drawString(text, x, y);
}

int16_t DisplayDriver::drawUtf8(const String& text, int16_t x, int16_t y,
                                uint8_t font, uint16_t foreground,
                                uint16_t background, uint8_t datum) {
    return drawUtf8(text.c_str(), x, y, font, foreground, background, datum);
}

void DisplayDriver::selectFont(uint8_t font) {
    const uint8_t smoothFont = font == 2 || font == 4 ? font : 0;
    if (smoothFont == loadedSmoothFont_) return;

    if (loadedSmoothFont_ != 0) tft_.unloadFont();
    if (smoothFont == 2) tft_.loadFont(NotoSansPl15);
    if (smoothFont == 4) tft_.loadFont(NotoSansPl24);
    loadedSmoothFont_ = smoothFont;
}
