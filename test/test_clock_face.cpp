#include "WordClockState.h"

#include "WordClockTypes/ClockType.hpp"

GLOBAL G = {};
uint32_t frontMatrix[MAX_ROW_SIZE] = {0};
uint8_t frontWordId[MAX_ROW_SIZE][MAX_COL_SIZE] = {{WORD_ID_NONE}};
uint8_t minuteArray = 0;
uint16_t minutePixelArray[4] = {0};
ClockType *usedClockType = nullptr;

#include "AllLayouts.gen.h"

#include "../src/ClockFace.cpp"

#include "support/check.h"

#include <initializer_list>

namespace {

struct Layout {
    const char *name;
    uint8_t id;
    ClockType *clockType;
};

const Layout LAYOUTS[] = {
#define X(name, id, var, i18n) {#name, id, &var},
    CLOCK_TYPES_LIST
#undef X
};

struct ExpectedHash {
    const char *name;
    uint32_t hash;
};

const ExpectedHash EXPECTED_HASHES[] = {
    {"BN9x8", 0xedcca1c5},
    {"Ch10x11", 0xdd81d045},
    {"Ch10x11V2", 0x35cf8e45},
    {"Ch10x11ZH", 0xe85b1a45},
    {"Eng08x08", 0xa6009dc5},
    {"Eng10x11", 0xf43cc545},
    {"Eng11x12", 0xebef8d45},
    {"Es08x08Cuarto", 0xafb8edc5},
    {"Es10x11", 0xeb6624c5},
    {"Fr10x11", 0x72ede285},
    {"Ger08x08", 0x149ccdc5},
    {"Ger08x08Viertel", 0xce86fdc5},
    {"Ger10x11", 0x22e29445},
    {"Ger10x11Alternative", 0x2984a7c5},
    {"Ger10x11bayerisch", 0x4b533e45},
    {"Ger10x11Clock", 0x50f4ed45},
    {"Ger10x11Mrrioes", 0xe7b6be05},
    {"Ger10x11Nero", 0x5b36f6e5},
    {"Ger10x11schwaebisch", 0x7dda40c5},
    {"Ger11x11", 0xa00b33e5},
    {"Ger11x11schwaebisch", 0x3675cc45},
    {"Ger11x11V2", 0xda9738c5},
    {"Ger11x11V3", 0xdb3e2785},
    {"Ger13x13", 0xde61c3c5},
    {"Ger16x8", 0x5f4550c5},
    {"Ger16x18", 0x2f616dc5},
    {"Ger22x11Weather", 0x38f9be45},
    {"Hu10x10", 0x91b3abc5},
    {"It10x11", 0x2819de45},
    {"Nl10x11", 0xddfcbcc5},
    {"Ro10x11", 0x0ff3e4c5},
    {"Ru10x11", 0x334f7dc5},
    {"Se10x11", 0x5852dbc5},
    {"Tr10x11", 0xe8aa8ec5},
};

const size_t LAYOUT_COUNT = sizeof(LAYOUTS) / sizeof(LAYOUTS[0]);

char message[160];

void clearFace() {
    memset(frontMatrix, 0, sizeof(frontMatrix));
    minuteArray = 0;
    memset(minutePixelArray, 0, sizeof(minutePixelArray));
}

void useLayout(const Layout &layout) {
    G = {};
    G.clockTypeDef = layout.id;
    usedClockType = layout.clockType;
}

void mix(uint32_t &hash, uint32_t value) {
    for (uint8_t i = 0; i < 4; i++) {
        hash ^= (value >> (8 * i)) & 0xFF;
        hash *= 16777619u;
    }
}

uint32_t hashEveryTimeAndVariant(const Layout &layout) {
    ClockFace face;
    uint32_t hash = 2166136261u;

    for (uint8_t languageMask = 0; languageMask < 32; languageMask++) {
        for (uint8_t itIs = 0; itIs < 5; itIs++) {
            for (uint8_t minuteVariant = 0; minuteVariant < 4;
                 minuteVariant++) {
                useLayout(layout);
                for (uint8_t bit = 0; bit < 5; bit++) {
                    G.languageVariant[bit] = (languageMask >> bit) & 1;
                }
                G.itIsVariant = static_cast<ItIsVariant>(itIs);
                G.minuteVariant = static_cast<MinuteVariant>(minuteVariant);

                for (uint8_t hour = 0; hour < 24; hour++) {
                    for (uint8_t minute = 0; minute < 60; minute++) {
                        clearFace();
                        face.show(hour, minute);
                        for (uint8_t row = 0; row < MAX_ROW_SIZE; row++) {
                            mix(hash, frontMatrix[row]);
                        }
                        mix(hash, minuteArray);
                        for (uint8_t i = 0; i < 4; i++) {
                            mix(hash, minutePixelArray[i]);
                        }
                    }
                }
            }
        }
    }
    return hash;
}

void everyLayoutKeepsItsWording() {
    for (size_t i = 0; i < LAYOUT_COUNT; i++) {
        const ExpectedHash *expected = nullptr;
        for (const ExpectedHash &candidate : EXPECTED_HASHES) {
            if (strcmp(candidate.name, LAYOUTS[i].name) == 0) {
                expected = &candidate;
            }
        }

        const uint32_t actual = hashEveryTimeAndVariant(LAYOUTS[i]);
        snprintf(message, sizeof(message),
                 "%s: wording of every time unchanged (hash 0x%08x)",
                 LAYOUTS[i].name, actual);
        check(expected != nullptr && expected->hash == actual, message);
    }
}

const Layout &layoutNamed(const char *name) {
    for (const Layout &layout : LAYOUTS) {
        if (strcmp(layout.name, name) == 0) {
            return layout;
        }
    }
    return LAYOUTS[0];
}

bool showsExactly(const char *name, uint8_t hour, uint8_t minute,
                  std::initializer_list<FrontWord> words) {
    useLayout(layoutNamed(name));

    clearFace();
    for (FrontWord word : words) {
        usedClockType->show(word);
    }
    uint32_t expected[MAX_ROW_SIZE];
    memcpy(expected, frontMatrix, sizeof(expected));

    clearFace();
    ClockFace face;
    face.show(hour, minute);

    return memcmp(expected, frontMatrix, sizeof(expected)) == 0;
}

void halfHourFollowsTheLanguage() {
    check(showsExactly("Ger10x11", 15, 30,
                       {FrontWord::es_ist, FrontWord::halb, FrontWord::hour_4}),
          "Ger10x11: 15:30 is \"halb vier\"");
    check(showsExactly("Eng10x11", 15, 30,
                       {FrontWord::es_ist, FrontWord::halb, FrontWord::nach,
                        FrontWord::hour_3}),
          "Eng10x11: 15:30 is \"half past three\"");
    check(showsExactly("Fr10x11", 15, 30,
                       {FrontWord::es_ist, FrontWord::hour_3, FrontWord::halb}),
          "Fr10x11: 15:30 is \"trois heures et demie\"");
    check(
        showsExactly("Tr10x11", 15, 30,
                     {FrontWord::es_ist, FrontWord::hour_15, FrontWord::halb}),
        "Tr10x11: 15:30 uses the base form of the hour");
}

void russianAlwaysNamesTheCurrentHour() {
    check(showsExactly("Ru10x11", 15, 40,
                       {FrontWord::es_ist___plural___, FrontWord::min_40,
                        FrontWord::hour_3}),
          "Ru10x11: 15:40 names three, not four");
}

void spanishItIsAgreesWithTheHour() {
    check(showsExactly("Es10x11", 13, 10,
                       {FrontWord::es_ist___plural___, FrontWord::min_10,
                        FrontWord::nach, FrontWord::hour_1}),
          "Es10x11: 13:10 is \"es la una\"");
    check(showsExactly("Es10x11", 15, 10,
                       {FrontWord::es_ist, FrontWord::min_10, FrontWord::nach,
                        FrontWord::hour_3}),
          "Es10x11: 15:10 is \"son las tres\"");
}

} // namespace

int main() {
    everyLayoutKeepsItsWording();
    halfHourFollowsTheLanguage();
    russianAlwaysNamesTheCurrentHour();
    spanishItIsAgreesWithTheHour();
    return report("clock face");
}
