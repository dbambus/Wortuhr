#include "ClockFace.h"

#include "WordClockState.h"
#include "WordClockTypes/ClockType.hpp"
#include <Arduino.h>

extern ClockType *usedClockType;

namespace {
const FrontWord HOUR_WORDS[] = {
    FrontWord::hour_0,  FrontWord::hour_1,  FrontWord::hour_2,
    FrontWord::hour_3,  FrontWord::hour_4,  FrontWord::hour_5,
    FrontWord::hour_6,  FrontWord::hour_7,  FrontWord::hour_8,
    FrontWord::hour_9,  FrontWord::hour_10, FrontWord::hour_11,
    FrontWord::hour_12, FrontWord::hour_13, FrontWord::hour_14,
    FrontWord::hour_15, FrontWord::hour_16, FrontWord::hour_17,
    FrontWord::hour_18, FrontWord::hour_19, FrontWord::hour_20,
    FrontWord::hour_21, FrontWord::hour_22, FrontWord::hour_23,
};

const uint8_t HOUR_WORD_COUNT = sizeof(HOUR_WORDS) / sizeof(HOUR_WORDS[0]);

const FrontWord REMAINING_MINUTE_WORDS[] = {
    FrontWord::m_num1,
    FrontWord::m_num2,
    FrontWord::m_num3,
    FrontWord::m_num4,
};

void showWord(FrontWord word) { usedClockType->show(word); }
} // namespace

//------------------------------------------------------------------------------

void ClockFace::show(uint8_t hour, uint8_t minute) {
    uint8_t offsetHour = 0;
    bool fullHour = false;

    showMinutes(minute, offsetHour, fullHour);
    showHour(hour + offsetHour, fullHour);
    showDaytimeWords(hour + offsetHour);
    showItIs(hour + offsetHour, minute);
}

//------------------------------------------------------------------------------
// Minutes
//------------------------------------------------------------------------------

void ClockFace::showMinutes(uint8_t minute, uint8_t &offsetHour,
                            bool &fullHour) {
    if (usedClockType->has60MinuteLayout()) {
        showMinutesOn60MinuteLayout(minute, fullHour);
    } else if (usedClockType->hasOnlyQuarterLayout()) {
        showMinutesOnQuarterLayout(minute, offsetHour);
    } else {
        showRemainingMinutes(minute);
        showMinutesInFiveMinuteSteps(minute / 5 * 5, offsetHour, fullHour);
    }
}

//------------------------------------------------------------------------------

void ClockFace::showMinutesOn60MinuteLayout(uint8_t minute, bool &fullHour) {
    showWord(FrontWord::uhr);

    if (minute == 0) {
        fullHour = true;
        return;
    }

    showWord(minuteWord(minute));
    showWord(FrontWord::und);
    showWord(minute == 1 ? FrontWord::minute : FrontWord::minuten);
}

//------------------------------------------------------------------------------

void ClockFace::showMinutesOnQuarterLayout(uint8_t minute,
                                           uint8_t &offsetHour) {
    if (8 <= minute && minute <= 22) {
        showQuarterPast(offsetHour);
    } else if (23 <= minute && minute <= 37) {
        showWord(FrontWord::halb);
        if (usedClockType->halfHourPhrase() ==
            HalfHourPhrase::HalfPastCurrentHour) {
            showWord(FrontWord::nach);
        } else {
            offsetHour = 1;
        }
    } else if (38 <= minute && minute <= 52) {
        showQuarterTo(offsetHour);
    } else if (53 <= minute) {
        offsetHour = 1;
    }
}

//------------------------------------------------------------------------------

void ClockFace::showMinutesInFiveMinuteSteps(uint8_t minute,
                                             uint8_t &offsetHour,
                                             bool &fullHour) {
    switch (minute) {
    case 0:
        showWord(FrontWord::uhr);
        fullHour = true;
        break;
    case 5:
    case 10:
        showWord(minuteWord(minute));
        showMinutePastWord();
        break;
    case 15:
        showQuarterPast(offsetHour);
        break;
    case 20:
        if (twentyIsUsed()) {
            showWord(FrontWord::min_20);
            showMinutePastWord();
        } else {
            showWord(FrontWord::min_10);
            showWord(FrontWord::vor);
            showWord(FrontWord::halb);
            offsetHour = 1;
        }
        break;
    case 25:
        if (usedClockType->hasTwentyfive()) {
            showWord(FrontWord::min_25);
            showMinutePastWord();
        } else {
            showWord(FrontWord::min_5);
            showWord(FrontWord::vor);
            showWord(FrontWord::halb);
            offsetHour = 1;
        }
        break;
    case 30:
        showHalfHour(offsetHour, fullHour);
        break;
    case 35:
        if (usedClockType->hasThirtyfive()) {
            showWord(FrontWord::min_35);
            showWord(FrontWord::nach);
        } else if (usedClockType->hasTwentyfive()) {
            showWord(FrontWord::min_25);
            showWord(FrontWord::vor);
            offsetHour = 1;
        } else {
            showWord(FrontWord::min_5);
            showWord(FrontWord::nach);
            showWord(FrontWord::halb);
            offsetHour = 1;
        }
        break;
    case 40:
        if (usedClockType->hasForty()) {
            showWord(FrontWord::min_40);
        } else if (twentyIsUsed()) {
            showWord(FrontWord::min_20);
            showWord(FrontWord::vor);
        } else {
            showWord(FrontWord::min_10);
            showWord(FrontWord::nach);
            showWord(FrontWord::halb);
        }
        break;
    case 45:
        if (usedClockType->namesCurrentHourAfterHalf()) {
            showWord(FrontWord::min_45);
            showWord(FrontWord::nach);
        } else {
            showQuarterTo(offsetHour);
        }
        break;
    case 50:
        if (usedClockType->hasFifty()) {
            showWord(FrontWord::min_50);
        } else {
            showWord(minuteWord(60 - minute));
            showWord(FrontWord::vor);
        }
        break;
    case 55:
        if (usedClockType->hasFiftyFive()) {
            showWord(FrontWord::min_55);
        } else {
            showWord(minuteWord(60 - minute));
            showWord(FrontWord::vor);
        }
        break;
    default:
        break;
    }

    if (minute >= 40 && minute != 45 &&
        !usedClockType->namesCurrentHourAfterHalf()) {
        offsetHour = 1;
    }

    if (usedClockType->alwaysNamesCurrentHour()) {
        offsetHour = 0;
    }
}

//------------------------------------------------------------------------------

void ClockFace::showHalfHour(uint8_t &offsetHour, bool &fullHour) {
    showWord(FrontWord::halb);

    switch (usedClockType->halfHourPhrase()) {
    case HalfHourPhrase::CurrentHourBaseFormAndHalf:
        fullHour = true;
        break;
    case HalfHourPhrase::HalfPastCurrentHour:
        showWord(FrontWord::nach);
        break;
    case HalfHourPhrase::CurrentHourAndHalf:
        break;
    case HalfHourPhrase::HalfToNextHour:
    default:
        offsetHour = 1;
        break;
    }
}

//------------------------------------------------------------------------------

void ClockFace::showQuarterPast(uint8_t &offsetHour) {
    if (G.languageVariant[ItIs15]) {
        showWord(FrontWord::viertel);
        offsetHour = 1;
        return;
    }

    if (G.languageVariant[EN_ShowAQuarter]) {
        showWord(FrontWord::a_quarter);
    }
    showWord(FrontWord::viertel);
    showWord(FrontWord::v_nach);
}

//------------------------------------------------------------------------------

void ClockFace::showQuarterTo(uint8_t &offsetHour) {
    if (dreiviertelIsUsed()) {
        showWord(FrontWord::dreiviertel);
    } else {
        if (G.languageVariant[EN_ShowAQuarter]) {
            showWord(FrontWord::a_quarter);
        }
        showWord(FrontWord::viertel);
        showWord(FrontWord::v_vor);
    }
    offsetHour = 1;
}

//------------------------------------------------------------------------------

void ClockFace::showMinutePastWord() {
    if (usedClockType->usesPastWordForMinutes()) {
        showWord(FrontWord::nach);
    }
}

//------------------------------------------------------------------------------

void ClockFace::showRemainingMinutes(uint8_t minute) {
    const uint8_t remaining = minute % 5;

    usedClockType->getMinuteArray(minutePixelArray, G.minuteVariant);
    minuteArray = 0;
    if (usedClockType->hasMinuteInWords() && remaining > 0) {
        minuteArray |= 1UL << (remaining - 1);
    } else {
        for (uint8_t i = 0; i < remaining; i++) {
            minuteArray |= 1UL << i;
        }
    }

    showSpecialWordBeen(remaining);
    showMinuteInWords(remaining);
}

//------------------------------------------------------------------------------

void ClockFace::showSpecialWordBeen(uint8_t minute) {
    if (!usedClockType->hasSpecialWordBeen()) {
        return;
    }

    showWord(minute == 0 ? FrontWord::nur : FrontWord::gewesen);
}

//------------------------------------------------------------------------------

void ClockFace::showMinuteInWords(uint8_t minute) {
    if (!minuteInWordsIsActive() || minute == 0) {
        return;
    }

    showWord(FrontWord::plus);
    showWord(FrontWord::minute);
    if (minute > 1) {
        showWord(FrontWord::minuten);
    }
    showWord(REMAINING_MINUTE_WORDS[minute - 1]);
}

//------------------------------------------------------------------------------

bool ClockFace::minuteInWordsIsActive() {
    return usedClockType->hasMinuteInWords() &&
           G.minuteVariant == MinuteVariant::InWords;
}

//------------------------------------------------------------------------------
// Hours
//------------------------------------------------------------------------------

void ClockFace::showHour(uint8_t hour, bool fullHour) {
    const bool midnight = usedClockType->hasMitternacht() && hour == 0;

    if (!usedClockType->has24HourLayout()) {
        hour %= 12;
    }

    const bool turkishBaseHour =
        usedClockType->usedLang() == LanguageAbbreviation::TR && fullHour;
    if (turkishBaseHour && hour != 0) {
        hour += 12;
    }

    if (hour == 0) {
        showWord(turkishBaseHour || midnight ? FrontWord::hour_0
                                             : FrontWord::hour_12);
    } else if (hour == 1 && !fullHour &&
               usedClockType->usedLang() == LanguageAbbreviation::DE) {
        showWord(FrontWord::eins);
    } else if (hour < HOUR_WORD_COUNT) {
        showWord(HOUR_WORDS[hour]);
    }
}

//------------------------------------------------------------------------------

void ClockFace::showDaytimeWords(uint8_t hour) {
    if (!usedClockType->hasDaytimeWords()) {
        return;
    }

    hour %= 24;

    if (hour == 0) {
        showWord(FrontWord::mitternachts);
    } else if (hour < 5) {
        showWord(FrontWord::day_night);
    } else if (hour < 10) {
        showWord(FrontWord::day_morning);
        showWord(FrontWord::day_early);
    } else if (hour < 12) {
        showWord(FrontWord::day_morning);
    } else if (hour < 14) {
        showWord(FrontWord::day_noon);
    } else if (hour >= 18) {
        showWord(FrontWord::day_evening);
    }
}

//------------------------------------------------------------------------------

void ClockFace::showItIs(uint8_t hour, uint8_t minute) {
    const uint8_t roundedMinute = minute / 5 * 5;

    if (itIsIsShown(roundedMinute)) {
        showWord(usedClockType->itIsWord(hour % 12, roundedMinute));
    }
}

//------------------------------------------------------------------------------

bool ClockFace::itIsIsShown(uint8_t minute) {
    switch (G.itIsVariant) {
    case ItIsVariant::Permanent:
        return true;
    case ItIsVariant::Hourly:
        return minute == 0;
    case ItIsVariant::HalfHourly:
        return minute % 30 == 0;
    case ItIsVariant::Quarterly:
        return minute % 15 == 0;
    case ItIsVariant::Off:
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

bool ClockFace::twentyIsUsed() {
    return usedClockType->hasTwenty() &&
           (G.languageVariant[ItIs20] || G.languageVariant[ItIs40] ||
            usedClockType->usedLang() == LanguageAbbreviation::ES ||
            usedClockType->usedLang() == LanguageAbbreviation::TR);
}

//------------------------------------------------------------------------------

bool ClockFace::dreiviertelIsUsed() {
    return usedClockType->hasDreiviertel() &&
           (usedClockType->usedLang() != LanguageAbbreviation::DE ||
            G.languageVariant[ItIs45]);
}

//------------------------------------------------------------------------------

FrontWord ClockFace::minuteWord(uint8_t minute) {
    if (minute == 0 || minute > 59) {
        Serial.println("[Error] minuteWord() out of Bounds");
        return FrontWord::error;
    }
    return static_cast<FrontWord>(minute);
}
