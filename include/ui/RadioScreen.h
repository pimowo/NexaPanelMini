#pragma once
#include "ui/ScreenBase.h"

class RadioScreen : public ScreenBase {
public:
    void draw(DisplayDriver& display, const AppState& state) override;
    void update(DisplayDriver& display, const AppState& state) override;

private:
    void drawContent(DisplayDriver& display, const AppState& state);
    uint32_t revision_ = UINT32_MAX;
};
