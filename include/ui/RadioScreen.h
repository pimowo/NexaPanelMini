#pragma once
#include "ui/ScreenBase.h"

enum class RadioAction : uint8_t {
    NONE,
    VOLUME_DOWN,
    VOLUME_UP,
    PREVIOUS,
    TOGGLE_PLAY,
    NEXT
};

class RadioScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;
    RadioAction actionAt(const TouchPoint& point) const;

private:
    void drawStatic(DisplayDriver& display);
    void drawStation(DisplayDriver& display, const AppState& state,
                     bool clearRegion = true);
    void drawArtist(DisplayDriver& display, const AppState& state,
                    bool clearRegion = true);
    void drawTitle(DisplayDriver& display, const AppState& state,
                   bool clearRegion = true);
    void drawPlayback(DisplayDriver& display, const AppState& state,
                      bool clearRegion = true);
    void drawVolume(DisplayDriver& display, const AppState& state,
                    bool clearRegion = true);
    void drawClippedText(DisplayDriver& display, const String& text,
                         int16_t y, bool clearRegion);
    void cacheState(const AppState& state);

    uint32_t revision_ = UINT32_MAX;
    String station_;
    String artist_;
    String title_;
    int volume_ = -1;
    bool playing_ = false;
    bool cacheValid_ = false;
};
