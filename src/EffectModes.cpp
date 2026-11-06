#include "EffectModes.h"

#include "Font.h"
#include "Led.h"
#include "WordClockTypes/ClockType.hpp"
#include <Arduino.h>

extern ClockType *usedClockType;
extern Led led;

#include "Render/ColorStage.h"

extern ColorStage colorStage;

void EffectModes::rainbow() {
    static uint16_t hue = 0;

    for (uint8_t row = 0; row < usedClockType->rowsWordMatrix(); row++) {
        for (uint8_t col = 0; col < usedClockType->colsWordMatrix(); col++) {
            led.setPixel(row, col,
                         HsbColor(hue / 360.f, 1.f, G.effectBri / 100.f));
        }
    }
    led.show();
    hue++;
    led.checkIfHueIsOutOfBound(hue);
}

//-----------------------------------------------------------------------------

void EffectModes::rainbowCycle() {
    static uint16_t hue = 0;
    const uint16_t numPixelsWordMatrix =
        usedClockType->rowsWordMatrix() * usedClockType->colsWordMatrix();
    const float hueStep = 360.f / numPixelsWordMatrix;
    uint16_t pixel = 0;

    const uint8_t rows = usedClockType->rowsWordMatrix();
    const uint8_t cols = usedClockType->colsWordMatrix();

    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < cols; col++) {
            const float displayedHue = fmodf(hue + pixel * hueStep, 360.f);
            led.setPixel(
                mirroredRow(row, rows), mirroredCol(col, cols),
                HsbColor(displayedHue / 360.f, 1.f, G.effectBri / 100.f));
            pixel++;
        }
    }
    led.show();
    hue++;
    led.checkIfHueIsOutOfBound(hue);
}

//------------------------------------------------------------------------------

namespace {

// Fire2012 by Mark Kriegsman, July 2012 - one simulation per matrix column.
constexpr uint8_t FIRE_SPARK_ROWS = 3;

uint8_t coolCell(uint8_t heat, uint8_t amount) {
    return (heat > amount) ? (heat - amount) : 0;
}

uint8_t igniteCell(uint8_t heat, uint8_t amount) {
    uint16_t sum = static_cast<uint16_t>(heat) + amount;
    return (sum > 255) ? 255 : static_cast<uint8_t>(sum);
}

HsbColor heatColor(uint8_t heat, float brightness) {
    constexpr float yellow = 60.f / 360.f;

    if (heat < 85) {
        return HsbColor(0.f, 1.f, brightness * heat / 85.f);
    }
    if (heat < 170) {
        return HsbColor(yellow * (heat - 85) / 85.f, 1.f, brightness);
    }
    return HsbColor(yellow, 1.f - (heat - 170) / 85.f, brightness);
}

} // namespace

void EffectModes::fire() {
    static uint8_t heat[MAX_ROW_SIZE][MAX_COL_SIZE];
    static uint8_t heatRows = 0;
    static uint8_t heatCols = 0;

    const uint8_t rows = usedClockType->rowsWordMatrix();
    const uint8_t cols = usedClockType->colsWordMatrix();

    if (rows == 0 || cols == 0) {
        return;
    }

    if (rows != heatRows || cols != heatCols) {
        memset(heat, 0, sizeof(heat));
        heatRows = rows;
        heatCols = cols;
    }

    const uint8_t cooling =
        static_cast<uint8_t>((G.fireCooling * 10u) / rows) + 2u;
    const uint8_t sparkRows = (rows < FIRE_SPARK_ROWS) ? rows : FIRE_SPARK_ROWS;
    const float brightness = G.effectBri / 100.f;

    for (uint8_t col = 0; col < cols; col++) {
        for (uint8_t cell = 0; cell < rows; cell++) {
            heat[cell][col] = coolCell(heat[cell][col], random(cooling));
        }

        for (uint8_t cell = rows - 1; cell >= 2; cell--) {
            uint16_t below = heat[cell - 1][col] + 2u * heat[cell - 2][col];
            heat[cell][col] = static_cast<uint8_t>(below / 3u);
        }

        if (random(256) < G.fireSparking) {
            const uint8_t cell = static_cast<uint8_t>(random(sparkRows));
            heat[cell][col] = igniteCell(heat[cell][col], random(160, 256));
        }

        for (uint8_t cell = 0; cell < rows; cell++) {
            led.setPixel(
                mirroredRow(static_cast<uint8_t>(rows - 1 - cell), rows),
                mirroredCol(col, cols), heatColor(heat[cell][col], brightness));
        }
    }

    led.show();
}

//------------------------------------------------------------------------------

bool EffectModes::getStaticScrollingTextInfo(const char *buf,
                                             StaticScrollingText &info) {
    uint16_t textStart = 0;
    uint16_t textEnd = strlen(buf);

    auto isTrimmedWhitespace = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    while (textStart < textEnd && isTrimmedWhitespace(buf[textStart])) {
        textStart++;
    }

    while (textEnd > textStart && isTrimmedWhitespace(buf[textEnd - 1])) {
        textEnd--;
    }

    uint16_t trimmedLen = textEnd - textStart;
    if (trimmedLen == 0 || trimmedLen > 2) {
        return false;
    }

    info.start = static_cast<uint8_t>(textStart);
    info.len = static_cast<uint8_t>(trimmedLen);
    info.onlyDigits = true;

    for (uint16_t i = textStart; i < textEnd; i++) {
        if (buf[i] < '0' || buf[i] > '9') {
            info.onlyDigits = false;
            break;
        }
    }

    if (!info.onlyDigits) {
        info.width = pgm_read_byte(&(fontWidth[normalSizeASCII]));
        info.height = pgm_read_byte(&(fontHeight[normalSizeASCII]));
        info.totalWidth = info.len * info.width + info.len - 1;

        if (usedClockType->colsWordMatrix() < info.totalWidth ||
            usedClockType->rowsWordMatrix() < info.height) {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------

void EffectModes::displayStaticScrollingText(const char *buf,
                                             const StaticScrollingText &info) {
    if (info.onlyDigits) {
        led.showNumbers(info.len == 1 ? ' ' : buf[info.start],
                        buf[info.start + info.len - 1]);
        return;
    }

    led.clearClock();

    uint8_t offsetRow = (usedClockType->rowsWordMatrix() - info.height) / 2;
    uint8_t offsetCol = (usedClockType->colsWordMatrix() - info.totalWidth) / 2;

    for (uint8_t charIndex = 0; charIndex < info.len; charIndex++) {
        uint8_t charOffsetCol = offsetCol + charIndex * (info.width + 1);
        unsigned char fontIndex =
            static_cast<unsigned char>(buf[info.start + charIndex]);

        for (uint8_t col = 0; col < info.width; col++) {
            for (uint8_t row = 0; row < info.height; row++) {
                led.setPixelForChar(col, row, charOffsetCol, offsetRow,
                                    fontIndex, normalSizeASCII);
            }
        }
    }

    led.mirrorFrontMatrixVertical();
    led.setbyFrontMatrix(Foreground);
    led.show();
}

//------------------------------------------------------------------------------

void EffectModes::scrollingText(const char *buf) {
    static uint8_t i = 0, ii = 0;
    StaticScrollingText staticText;

    if (getStaticScrollingTextInfo(buf, staticText)) {
        i = 0;
        ii = 0;
        displayStaticScrollingText(buf, staticText);
        return;
    }

    uint8_t offsetRow = (usedClockType->rowsWordMatrix() -
                         pgm_read_byte(&(fontHeight[normalSizeASCII]))) /
                        2;

    uint16_t len = strlen(buf);
    uint8_t width = pgm_read_byte(&(fontWidth[normalSizeASCII]));
    uint8_t charsToFlush = (usedClockType->colsWordMatrix() / (width + 1)) + 1;

    if (ii >= len + charsToFlush) {
        ii = 0;
        i = 0;
    }

    uint8_t fontIndex = (ii < len) ? buf[ii] : ' ';

    led.setbyFrontMatrix(Foreground); // Needed for Mirrored Display
    led.shiftColumnToRight();
    led.clearFrontExeptofFontspace(offsetRow);

    if (i < width) {
        for (uint8_t row = 0;
             row < pgm_read_byte(&(fontHeight[normalSizeASCII])); row++) {
            usedClockType->setFrontMatrixPixel(
                row + offsetRow, 0,
                pgm_read_byte(&(font_7x5[fontIndex][i])) & (1u << row));
        }
    } else {
        for (uint8_t row = 0;
             row < pgm_read_byte(&(fontHeight[normalSizeASCII])); row++) {
            usedClockType->setFrontMatrixPixel(row + offsetRow, 0, false);
        }
    }

    led.setbyFrontMatrix(Foreground);
    led.show();

    i++;
    if (i >= width + 1) // +1 for spacing
    {
        i = 0;
        ii++;
        if (ii >= len + charsToFlush) {
            ii = 0;
        }
    }
}

//------------------------------------------------------------------------------

void EffectModes::displaySymbols(BitmapSymbol symbolNum) {
    if (symbolNum >= BitmapSymbol::MAX_BITMAP_SYMBOLS) {
        symbolNum = BitmapSymbol::HEART;
    }

    // This mode has its own brightness, so both ends of the ramp are dimmed to
    // it rather than read through the display brightness.
    HsbColor color = G.color[Foreground];
    color.B = G.effectBri / 100.f;

    if (colorStage.foregroundIsGradient()) {
        HsbColor gradientEnd = G.color[GradientEnd];
        gradientEnd.B = color.B;
        led.setBitmapSymbol(symbolNum, color, gradientEnd);
        return;
    }

    led.setBitmapSymbol(symbolNum, color);
}
