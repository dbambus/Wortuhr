#pragma once

#include "WordClockTypes/FrontWord.h"
#include <stdint.h>

class ClockFace {
public:
    void show(uint8_t hour, uint8_t minute);

private:
    void showMinutes(uint8_t minute, uint8_t &offsetHour, bool &fullHour);
    void showMinutesOn60MinuteLayout(uint8_t minute, bool &fullHour);
    void showMinutesOnQuarterLayout(uint8_t minute, uint8_t &offsetHour);
    void showMinutesInFiveMinuteSteps(uint8_t minute, uint8_t &offsetHour,
                                      bool &fullHour);
    void showHalfHour(uint8_t &offsetHour, bool &fullHour);
    void showQuarterPast(uint8_t &offsetHour);
    void showQuarterTo(uint8_t &offsetHour);
    void showMinutePastWord();

    void showRemainingMinutes(uint8_t minute);
    void showSpecialWordBeen(uint8_t minute);
    void showMinuteInWords(uint8_t minute);
    bool minuteInWordsIsActive();

    void showHour(uint8_t hour, bool fullHour);
    void showDaytimeWords(uint8_t hour);
    void showItIs(uint8_t hour, uint8_t minute);
    bool itIsIsShown(uint8_t minute);

    bool twentyIsUsed();
    bool dreiviertelIsUsed();
    FrontWord minuteWord(uint8_t minute);
};
