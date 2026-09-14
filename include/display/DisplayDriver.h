#pragma once
#include <TFT_eSPI.h>

class DisplayDriver {
public:
    void begin();
    TFT_eSPI& tft();
    int16_t drawUtf8(const char* text, int16_t x, int16_t y, uint8_t font,
                     uint16_t foreground, uint16_t background,
                     uint8_t datum);
    int16_t drawUtf8(const String& text, int16_t x, int16_t y, uint8_t font,
                     uint16_t foreground, uint16_t background,
                     uint8_t datum);
#ifdef UI_DRAW_DIAGNOSTICS
    void resetDrawDiagnostics();
    void printDrawDiagnostics(uint32_t totalUs) const;
#endif

private:
    void selectFont(uint8_t font);

    TFT_eSPI tft_;
    uint8_t loadedSmoothFont_ = 0;
#ifdef UI_DRAW_DIAGNOSTICS
    uint32_t fontSelectUs_ = 0;
    uint32_t textDrawUs_ = 0;
    uint16_t textDrawCount_ = 0;
    uint16_t fontSwitchCount_ = 0;
#endif
};
