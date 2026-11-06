#pragma once

#include "WordClockState.h"
#include <stdint.h>

class EffectModes {
public:
    void rainbow();
    void rainbowCycle();
    void fire();
    void scrollingText(const char *buf);
    void displaySymbols(BitmapSymbol bitmapSymbol);

private:
    struct StaticScrollingText {
        uint8_t start = 0;
        uint8_t len = 0;
        uint8_t width = 0;
        uint8_t height = 0;
        uint8_t totalWidth = 0;
        bool onlyDigits = true;
    };

    bool getStaticScrollingTextInfo(const char *buf, StaticScrollingText &info);
    void displayStaticScrollingText(const char *buf,
                                    const StaticScrollingText &info);
};
