#include "Animation.h"
#include "NeoMultiFeature.hpp"
#include "Uhr.h"
#include "Uhrtypes/Uhrtype.hpp"
#include "clockWork.h"
#include "font.h"
#include "icons.h"
#include <Arduino.h>

extern NeoPixelBus<NeoMultiFeature, Neo800KbpsMethod> *strip_RGB;
extern NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> *strip_RGBW;

extern iUhrType *usedUhrType;
extern Animation *animation;

void ClockWork::ledSetPixel(uint8_t rr, uint8_t gg, uint8_t bb, uint8_t ww,
                            uint16_t i) {

    if (G.Colortype == Grbw) {
        strip_RGBW->SetPixelColor(i, RgbwColor(rr, gg, bb, ww));
    } else {
        strip_RGB->SetPixelColor(i, RgbColor(rr, gg, bb));
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetPixelHsb(uint16_t ledIndex, float hue, float sat,
                               float bri, uint8_t alpha = 0) {
    HsbColor hsbColor = HsbColor(hue / 360, sat / 100, bri / 100);

    if (G.Colortype == Grbw) {
        RgbColor rgbColor = RgbColor(hsbColor);

        strip_RGBW->SetPixelColor(
            ledIndex, RgbwColor(rgbColor.R, rgbColor.G, rgbColor.B, alpha));
    } else {
        strip_RGB->SetPixelColor(ledIndex, hsbColor);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetPixelColorObject(uint16_t i, RgbColor color) {

    if (G.Colortype == Grbw) {
        strip_RGBW->SetPixelColor(i, RgbwColor(color));
    } else {
        strip_RGB->SetPixelColor(i, color);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetPixelColorObjectRgbw(uint16_t i, RgbwColor color) {
    strip_RGBW->SetPixelColor(i, RgbwColor(color));
}

//------------------------------------------------------------------------------

RgbColor ClockWork::ledGetPixel(uint16_t i) {
    if (G.Colortype == Grbw) {
        RgbwColor rgbw = strip_RGBW->GetPixelColor(i);
        return RgbColor(rgbw.R, rgbw.G, rgbw.B);
    }
    return strip_RGB->GetPixelColor(i);
}

//------------------------------------------------------------------------------

RgbwColor ClockWork::ledGetPixelRgbw(uint16_t i) {
    return strip_RGBW->GetPixelColor(i);
}

//------------------------------------------------------------------------------
// Helligkeitsregelung nach Uhrzeiten oder per LDR
//------------------------------------------------------------------------------
uint8_t ClockWork::setBrightnessAuto(uint8_t val) {
    // G.hh enthaelt zeitabhaengige Helligkeitswerte in %
    uint16_t u16 = (val * G.hh) / 100;
    return (static_cast<uint8_t>((u16 * ldrVal) / 100));
}

//------------------------------------------------------------------------------

void ClockWork::setBrightnessLdr(uint8_t &rr, uint8_t &gg, uint8_t &bb,
                                 uint8_t &ww, uint8_t position) {
    if (G.autoLdrEnabled) {
        rr = setBrightnessAuto(G.rgb[position][0]);
        gg = setBrightnessAuto(G.rgb[position][1]);
        bb = setBrightnessAuto(G.rgb[position][2]);
        ww = setBrightnessAuto(G.rgb[position][3]);
    } else {
        if (G.ldr == 1) {
            rr = G.rgb[position][0] * ldrVal / 100;
            gg = G.rgb[position][1] * ldrVal / 100;
            bb = G.rgb[position][2] * ldrVal / 100;
            ww = G.rgb[position][3] * ldrVal / 100;
        } else {
            rr = G.rgb[position][0] * G.hh / 100;
            gg = G.rgb[position][1] * G.hh / 100;
            bb = G.rgb[position][2] * G.hh / 100;
            ww = G.rgb[position][3] * G.hh / 100;
        }
    }
}

//------------------------------------------------------------------------------

void ClockWork::setBrightness(uint8_t &rr, uint8_t &gg, uint8_t &bb,
                              uint8_t &ww, uint8_t position,
                              uint8_t percentage = 100) {
    rr = G.rgb[position][0] * percentage / 100;
    gg = G.rgb[position][1] * percentage / 100;
    bb = G.rgb[position][2] * percentage / 100;
    ww = G.rgb[position][3] * percentage / 100;
    uint16_t zz = rr + gg + bb;
    if (zz > 150) {
        zz = zz * 10 / 150;
        rr = rr * 10 / zz;
        gg = gg * 10 / zz;
        bb = bb * 10 / zz;
        ww = ww * 10 / zz;
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledShow() {
    if (G.Colortype == Grbw) {
        strip_RGBW->Show();
    } else {
        strip_RGB->Show();
    }
}

//------------------------------------------------------------------------------

inline void ClockWork::ledClearPixel(uint16_t i) {
    if (G.Colortype == Grbw) {
        strip_RGBW->SetPixelColor(i, 0);
    } else {
        strip_RGB->SetPixelColor(i, 0);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledClear() {
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        Word_array[i] = 500;
        ledClearPixel(i);
    }
}

//------------------------------------------------------------------------------

inline void ClockWork::ledClearClock() {
    for (uint16_t i = 0; i < usedUhrType->NUM_SMATRIX(); i++) {
        ledClearPixel(usedUhrType->getSMatrix(i));
    }
}

//------------------------------------------------------------------------------

inline void ClockWork::ledClearRow(uint8_t row) {
    for (uint8_t i = 0; i < usedUhrType->COLS_MATRIX(); i++) {
        ledClearPixel(usedUhrType->getFrontMatrix(row, i));
    }
}

//------------------------------------------------------------------------------

inline void ClockWork::ledClearFrontExeptofFontspace(uint8_t offsetRow) {
    for (uint8_t i = 0; i < offsetRow; i++) {
        ledClearRow(i);
    }

    for (uint8_t i = usedUhrType->ROWS_MATRIX() - 1; i > offsetRow + fontHeight;
         i--) {
        ledClearRow(i - 1);
    }
}

//------------------------------------------------------------------------------

inline void ClockWork::ledClearFrame() {
    for (uint16_t i = 0; i < usedUhrType->NUM_RMATRIX(); i++) {
        ledClearPixel(usedUhrType->getRMatrix(i));
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSet(bool changed) {
    uint8_t rr, gg, bb, ww;
    uint8_t r2, g2, b2, w2;
    setBrightnessLdr(rr, gg, bb, ww, Foreground);
    setBrightnessLdr(r2, g2, b2, w2, Background);
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        if (Word_array_old[i] < usedUhrType->NUM_PIXELS()) {
            // foreground
            ledSetPixel(rr, gg, bb, ww, i);
        } else {
            // background
            ledSetPixel(r2, g2, b2, w2, i);
        }
    }
    if (animation->led_show_notify(changed, _minute)) {
        ledShow();
    }
}

//------------------------------------------------------------------------------

void ClockWork::copyClockface(const uint16_t source[], uint16_t destination[]) {
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        destination[i] = source[i];
    }
}

//------------------------------------------------------------------------------

bool ClockWork::changesInClockface() {
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        if (Word_array[i] != Word_array_old[i]) {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------

void ClockWork::ledSetIcon(uint8_t num_icon, uint8_t brightness = 100,
                           bool rgb_icon = false) {
    uint8_t rr, gg, bb, ww;
    setBrightness(rr, gg, bb, ww, Foreground, brightness);
    for (uint8_t col = 0; col < GRAFIK_11X10_COLS; col++) {
        if (rgb_icon) {
            rr = col < 3 ? 255 : 0;
            gg = (col > 3) && (col < 7) ? 255 : 0;
            bb = col > 7 ? 255 : 0;
            ww = 0;
        }
        for (uint8_t row = 0; row < GRAFIK_11X10_ROWS; row++) {
            if (pgm_read_word(&(grafik_11x10[num_icon][row])) &
                (1 << (GRAFIK_11X10_COLS - 1 - col))) {
                ledSetPixel(rr, gg, bb, ww,
                            usedUhrType->getFrontMatrix(row, col));
            } else {
                ledClearPixel(usedUhrType->getFrontMatrix(row, col));
            }
        }
    }
    ledShow();
}

//------------------------------------------------------------------------------

void ClockWork::ledSingle(uint8_t wait) {
    float hue;

    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        hue = 360.0 * i / (usedUhrType->NUM_PIXELS() - 1);
        hue = hue + 360.0 / usedUhrType->NUM_PIXELS();
        checkIfHueIsOutOfBound(hue);

        ledClear();
        ledSetPixelHsb(i, hue, 100, 100);
        ledShow();
        delay(wait);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetAllPixels(uint8_t rr, uint8_t gg, uint8_t bb,
                                uint8_t ww) {
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        ledSetPixel(rr, gg, bb, ww, i);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetColor() {
    uint8_t rr, gg, bb, ww;
    setBrightness(rr, gg, bb, ww, Effect);
    ledSetAllPixels(rr, gg, bb, ww);
}

//------------------------------------------------------------------------------
// Routine Helligkeitsregelung
//------------------------------------------------------------------------------

void ClockWork::doLDRLogic() {
    int16_t lux = analogRead(A0); // Range 0-1023
    uint8_t ldrValOld = ldrVal;

    if (G.autoLdrEnabled) {
        lux /= 4;
        int minimum = min(G.autoLdrBright, G.autoLdrDark);
        int maximum = max(G.autoLdrBright, G.autoLdrDark);
        if (lux >= maximum)
            lux = maximum;
        if (lux <= minimum)
            lux = minimum;
        if (G.autoLdrDark == G.autoLdrBright) {
            // map() wuerde mit division durch 0 crashen
            ldrVal = 100;
        } else {
            ldrVal = map(lux, G.autoLdrDark, G.autoLdrBright, 10, 100);
        }
    } else {
        if (G.ldr == 1) {
            lux = lux - (G.ldrCal * 20);
            if (lux >= 900) {
                lux = 900;
            } // Maximale Helligkeit
            if (lux <= 1) {
                lux = 1;
            } // Minimale Helligkeit
            ldrVal = map(lux, 1, 900, 1, 100);
        }
    }
    if (ldrValOld != ldrVal) {
        // lass den LDR sofort wirken
        ledSet();
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetFrameColor() {
    uint8_t rr, gg, bb, ww;
    setBrightness(rr, gg, bb, ww, Frame);

    for (uint16_t i = 0; i < usedUhrType->NUM_RMATRIX(); i++) {
        ledSetPixel(rr, gg, bb, ww, usedUhrType->getRMatrix(i));
    }
}

//------------------------------------------------------------------------------

void ClockWork::rainbow() {
    static float hue = 0;

    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        ledSetPixelHsb(i, hue, 100, G.hell);
    }
    ledShow();
    hue++;
    checkIfHueIsOutOfBound(hue);
}

//-----------------------------------------------------------------------------

void ClockWork::rainbowCycle() {
    static float hue = 0;
    float displayedHue;

    displayedHue = hue;
    for (uint16_t i = 0; i < usedUhrType->NUM_SMATRIX(); i++) {
        ledSetPixelHsb(usedUhrType->getSMatrix(i), displayedHue, 100, G.hell);
        displayedHue = displayedHue + 360.0 / usedUhrType->NUM_SMATRIX();
        checkIfHueIsOutOfBound(displayedHue);
    }
    ledShow();
    hue++;
    checkIfHueIsOutOfBound(hue);
}

//------------------------------------------------------------------------------

void ClockWork::ledShiftColumnToRight() {
    for (uint8_t col = 0; col < usedUhrType->COLS_MATRIX() - 1; col++) {
        for (uint8_t row = 0;
             row < usedUhrType->ROWS_MATRIX() - 1 /* Only Front*/; row++) {
            if (G.Colortype == Grbw) {
                ledSetPixelColorObjectRgbw(
                    usedUhrType->getFrontMatrix(row, col),
                    ledGetPixelRgbw(usedUhrType->getFrontMatrix(row, col + 1)));
            } else {
                ledSetPixelColorObject(
                    usedUhrType->getFrontMatrix(row, col),
                    ledGetPixel(usedUhrType->getFrontMatrix(row, col + 1)));
            }
        }
    }
}

//------------------------------------------------------------------------------

void ClockWork::scrollingText(const char *buf) {
    static uint8_t i = 0, ii = 0;
    uint8_t offsetRow = (usedUhrType->ROWS_MATRIX() - fontHeight - 1) / 2;
    uint8_t fontIndex = buf[ii];

    ledShiftColumnToRight();
    ledClearFrontExeptofFontspace(offsetRow);

    if (i < fontWidth) {
        for (uint8_t row = 0; row < fontHeight; row++) {
            if (pgm_read_byte(&(font_7x5[fontIndex][i])) & (1u << row)) {
                ledSetPixel(
                    G.rgb[Effect][0], G.rgb[Effect][1], G.rgb[Effect][2],
                    G.rgb[Effect][3],
                    usedUhrType->getFrontMatrix(
                        row + offsetRow, usedUhrType->COLS_MATRIX() - 1));
            } else {
                ledClearPixel(usedUhrType->getFrontMatrix(
                    row + offsetRow, usedUhrType->COLS_MATRIX() - 1));
            }
        }
    } else {
        for (uint8_t row = 0; row < fontHeight; row++) {
            ledClearPixel(usedUhrType->getFrontMatrix(
                row + offsetRow, usedUhrType->COLS_MATRIX() - 1));
        }
    }
    ledShow();

    i++;
    if (i > fontWidth) {
        i = 0;
        ii++;
        if (ii > strlen(buf)) {
            ii = 0;
        }
    }
}

//------------------------------------------------------------------------------

void ClockWork::showIp(const char *buf) {
    uint8_t StringLength = strlen(buf);
    StringLength = StringLength * 6; // Times 6, because thats the length of a
                                     // char in the 7x5 font plus spacing
    for (uint16_t i = 0; i <= StringLength; i++) {
        scrollingText(buf);
        delay(200);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledSetPixelForChar(uint8_t col, uint8_t row, uint8_t offsetCol,
                                   uint8_t offsetRow,
                                   unsigned char unsigned_d1) {
    if (pgm_read_byte(&(font_7x5[unsigned_d1][col])) & (1u << row)) {
        ledSetPixel(
            G.rgb[Effect][0], G.rgb[Effect][1], G.rgb[Effect][2],
            G.rgb[Effect][3],
            usedUhrType->getFrontMatrix(row + offsetRow, col + offsetCol));
    }
}

//------------------------------------------------------------------------------
// show signal-strenght by using different brightness for the individual rings
//------------------------------------------------------------------------------

void ClockWork::showWifiSignalStrength(int strength) {
    if (strength <= 100) {
        ledSetIcon(WLAN100, 100);
    } else if (strength <= 60) {
        ledSetIcon(WLAN60, 60);
    } else if (strength <= 30) {
        ledSetIcon(WLAN30, 30);
    }
}

//------------------------------------------------------------------------------

void ClockWork::ledShowNumbers(const char d1, const char d2) {
    ledClearClock();
    static uint8_t offsetLetter0 = 0;
    static uint8_t offsetLetter1 = fontWidth + 1;
    uint8_t offsetRow = (usedUhrType->ROWS_MATRIX() - fontHeight - 1) / 2;

    if (usedUhrType->has24HourLayout()) {
        offsetLetter0 = 3;
        offsetLetter1 = fontWidth + 4;
    }

    for (uint8_t col = 0; col < fontWidth; col++) {
        for (uint8_t row = 0; row < fontHeight; row++) {
            // 1. Zahl ohne Offset
            ledSetPixelForChar(col, row, offsetLetter0, offsetRow,
                               static_cast<unsigned char>(d1));
            // 2. Zahl mit Offset
            ledSetPixelForChar(col, row, offsetLetter1, offsetRow,
                               static_cast<unsigned char>(d2));
        }
    }
    ledShow();
}

//------------------------------------------------------------------------------

void ClockWork::clockSetHour(const uint8_t std, const uint8_t voll) {
    switch (std) {
    case 0:
        usedUhrType->show(h_zwoelf);
        break;
    case 1:
        if (voll == 1) {
            usedUhrType->show(h_ein);
        } else {
            usedUhrType->show(eins);
        }
        break;
    case 2:
        usedUhrType->show(h_zwei);
        break;
    case 3:
        usedUhrType->show(h_drei);
        break;
    case 4:
        usedUhrType->show(h_vier);
        break;
    case 5:
        usedUhrType->show(h_fuenf);
        break;
    case 6:
        usedUhrType->show(h_sechs);
        break;
    case 7:
        usedUhrType->show(h_sieben);
        break;
    case 8:
        usedUhrType->show(h_acht);
        break;
    case 9:
        usedUhrType->show(h_neun);
        break;
    case 10:
        usedUhrType->show(h_zehn);
        break;
    case 11:
        usedUhrType->show(h_elf);
        break;
    case 12:
        usedUhrType->show(h_zwoelf);
        break;
    case 13:
        if (voll == 1) {
            usedUhrType->show(h_ein);
        } else {
            usedUhrType->show(eins);
        }
        break;
    case 14:
        usedUhrType->show(h_zwei);
        break;
    case 15:
        usedUhrType->show(h_drei);
        break;
    case 16:
        usedUhrType->show(h_vier);
        break;
    case 17:
        usedUhrType->show(h_fuenf);
        break;
    case 18:
        usedUhrType->show(h_sechs);
        break;
    case 19:
        usedUhrType->show(h_sieben);
        break;
    case 20:
        usedUhrType->show(h_acht);
        break;
    case 21:
        usedUhrType->show(h_neun);
        break;
    case 22:
        usedUhrType->show(h_zehn);
        break;
    case 23:
        usedUhrType->show(h_elf);
        break;
    case 24:
        usedUhrType->show(h_zwoelf);
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::clockShowMinute(uint8_t min) {
    if (G.zeige_min > 0) {
        // Minuten / Sekunden-Animation
        // Minute (1-4)  ermitteln
        while (min > 4) {
            min -= 5;
        }

        if (min > 0) {
            Word_array[usedUhrType->getMinArr(G.zeige_min - 1, 0)] =
                usedUhrType->getMinArr(G.zeige_min - 1, 0);
        }
        if (min > 1) {
            Word_array[usedUhrType->getMinArr(G.zeige_min - 1, 1)] =
                usedUhrType->getMinArr(G.zeige_min - 1, 1);
        }
        if (min > 2) {
            Word_array[usedUhrType->getMinArr(G.zeige_min - 1, 2)] =
                usedUhrType->getMinArr(G.zeige_min - 1, 2);
        }
        if (min > 3) {
            Word_array[usedUhrType->getMinArr(G.zeige_min - 1, 3)] =
                usedUhrType->getMinArr(G.zeige_min - 1, 3);
        }
    }
}

//------------------------------------------------------------------------------

void ClockWork::clockSetMinute(uint8_t min, uint8_t &offsetH, uint8_t &voll) {
    if (!usedUhrType->has24HourLayout()) {
        clockShowMinute(min);
        min /= 5;
        min *= 5;
    }
    switch (min) {
    case 0: // volle Stunde
        usedUhrType->show(uhr);
        voll = 1;
        break;
    case 1:
        usedUhrType->show(m_eine);
        usedUhrType->show(minute);
        usedUhrType->show(nach);
        break;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        usedUhrType->show(min);
        usedUhrType->show(minuten);
        usedUhrType->show(nach);
        break;
    case 15: // viertel nach
        if (G.Sprachvariation[ItIs15]) {
            usedUhrType->show(viertel);
            offsetH = 1;
        } else {
            usedUhrType->show(viertel);
            usedUhrType->show(v_nach);
        }
        break;
    case 16:
    case 17:
    case 18:
    case 19:
        usedUhrType->show(min);
        usedUhrType->show(nach);
        break;
    case 20: // 20 nach
        if (!usedUhrType->hasZwanzig() || G.Sprachvariation[ItIs20]) {
            usedUhrType->show(zehn);
            usedUhrType->show(vor);
            usedUhrType->show(halb);
            offsetH = 1;
        } else {
            usedUhrType->show(zwanzig);
            usedUhrType->show(nach);
        }
        break;
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
        usedUhrType->show(30 - min);
        usedUhrType->show(vor);
        usedUhrType->show(halb);
        offsetH = 1;
        break;
    case 29:
        usedUhrType->show(m_eine);
        usedUhrType->show(minute);
        usedUhrType->show(vor);
        usedUhrType->show(halb);
        offsetH = 1;
        break;
    case 30: // halb
        usedUhrType->show(halb);
        offsetH = 1;
        break;
    case 31:
        usedUhrType->show(m_eine);
        usedUhrType->show(minute);
        usedUhrType->show(nach);
        usedUhrType->show(halb);
        offsetH = 1;
        break;
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
        usedUhrType->show(min - 30);
        usedUhrType->show(nach);
        usedUhrType->show(halb);
        offsetH = 1;
        break;
    case 40: // 20 vor
        if (!usedUhrType->hasZwanzig() || G.Sprachvariation[ItIs40]) {
            usedUhrType->show(zehn);
            usedUhrType->show(nach);
            usedUhrType->show(halb);
        } else {
            usedUhrType->show(zwanzig);
            usedUhrType->show(vor);
        }
        offsetH = 1;
        break;
    case 41:
    case 42:
    case 43:
    case 44:
        usedUhrType->show(60 - min);
        usedUhrType->show(vor);
        offsetH = 1;
        break;
    case 45: // viertel vor
        if (usedUhrType->hasDreiviertel() && G.Sprachvariation[ItIs45]) {
            usedUhrType->show(dreiviertel);
        } else {
            usedUhrType->show(viertel);
            usedUhrType->show(v_vor);
        }
        offsetH = 1;
        break;
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
        usedUhrType->show(60 - min);
        usedUhrType->show(vor);
        offsetH = 1;
        break;
    case 56:
    case 57:
    case 58:
        usedUhrType->show(60 - min);
        usedUhrType->show(minuten);
        usedUhrType->show(vor);
        offsetH = 1;
        break;
    case 59:
        usedUhrType->show(m_eine);
        usedUhrType->show(minute);
        usedUhrType->show(vor);
        offsetH = 1;
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

static void countdownToMidnight() {
    Serial.printf("Count down: %d\n", 60 - _sekunde);
    switch (_sekunde) {
    case 50:
        usedUhrType->show(zehn);
        break;
    case 51:
        usedUhrType->show(h_neun);
        break;
    case 52:
        usedUhrType->show(h_acht);
        break;
    case 53:
        usedUhrType->show(h_sieben);
        break;
    case 54:
        usedUhrType->show(h_sechs);
        break;
    case 55:
        usedUhrType->show(fuenf);
        break;
    case 56:
        usedUhrType->show(h_vier);
        break;
    case 57:
        usedUhrType->show(h_drei);
        break;
    case 58:
        usedUhrType->show(h_zwei);
        break;
    case 59:
        usedUhrType->show(eins);
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::clockSetClock() {
    uhrzeit = 0;

    if (!G.Sprachvariation[NotShowItIs]) {
        usedUhrType->show(es_ist);
    }

    uint8_t offsetH = 0;
    uint8_t voll = 0;

    clockSetMinute(_minute, offsetH, voll);
    clockSetHour(_stunde + offsetH, voll);
}

//------------------------------------------------------------------------------

void ClockWork::ledShowSeconds() {
    uint8_t rr, gg, bb, ww;
    setBrightness(rr, gg, bb, ww, Effect);

    ledSetPixel(rr, gg, bb, ww, usedUhrType->getRMatrix(_sekunde48));
}

//------------------------------------------------------------------------------
// Wetterdaten anzeigen
//------------------------------------------------------------------------------

void ClockWork::clockShowWeather() {

    switch (wetterswitch) {
        // +6h
    case 1: {
        switch (wstunde) {
        case 1:
            usedUhrType->show(w_mittag);
            break;
        case 2:
            usedUhrType->show(w_abend);
            break;
        case 3:
            usedUhrType->show(w_nacht);
            break;
        case 4: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_frueh);
        } break;
        }
        switch (wtemp_6) {
        case 30: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_dreissig);
            usedUhrType->show(w_grad);
        } break;
        case 25: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 20: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 15: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 10: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 5: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case 1: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -1: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -5: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case -10: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -15: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -20: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case -25: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        }
        switch (wwetter_6) {
        case 200:
            usedUhrType->show(w_gewitter);
            break;
        case 300:
            usedUhrType->show(w_regen);
            break;
        case 500:
            usedUhrType->show(w_regen);
            break;
        case 600:
            usedUhrType->show(w_schnee);
            break;
        case 700:
            usedUhrType->show(w_warnung);
            break;
        case 800:
            usedUhrType->show(w_klar);
            break;
        case 801:
            usedUhrType->show(w_wolken);
            break;
        }
    } break;
        // +12h
    case 2: {
        switch (wstunde) {
        case 1:
            usedUhrType->show(w_abend);
            break;
        case 2:
            usedUhrType->show(w_nacht);
            break;
        case 3: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_frueh);
        } break;
        case 4: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_mittag);
        } break;
        }
        switch (wtemp_12) {
        case 30: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_dreissig);
            usedUhrType->show(w_grad);
        } break;
        case 25: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 20: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 15: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 10: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 5: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case 1: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -1: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -5: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case -10: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -15: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -20: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case -25: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        }
        switch (wwetter_12) {
        case 200:
            usedUhrType->show(w_gewitter);
            break;
        case 300:
            usedUhrType->show(w_regen);
            break;
        case 500:
            usedUhrType->show(w_regen);
            break;
        case 600:
            usedUhrType->show(w_schnee);
            break;
        case 700:
            usedUhrType->show(w_warnung);
            break;
        case 800:
            usedUhrType->show(w_klar);
            break;
        case 801:
            usedUhrType->show(w_wolken);
            break;
        }

    } break;
        // +18h
    case 3: {
        switch (wstunde) {
        case 1:
            usedUhrType->show(w_nacht);
            break;
        case 2: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_frueh);
        } break;
        case 3: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_mittag);
        } break;
        case 4: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_abend);
        } break;
        }
        switch (wtemp_18) {
        case 30: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_dreissig);
            usedUhrType->show(w_grad);
        } break;
        case 25: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 20: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 15: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 10: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 5: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case 1: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -1: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -5: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case -10: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -15: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -20: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case -25: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        }
        switch (wwetter_18) {
        case 200:
            usedUhrType->show(w_gewitter);
            break;
        case 300:
            usedUhrType->show(w_regen);
            break;
        case 500:
            usedUhrType->show(w_regen);
            break;
        case 600:
            usedUhrType->show(w_schnee);
            break;
        case 700:
            usedUhrType->show(w_warnung);
            break;
        case 800:
            usedUhrType->show(w_klar);
            break;
        case 801:
            usedUhrType->show(w_wolken);
            break;
        }

    } break;
        // +24h
    case 4: {
        switch (wstunde) {
        case 1: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_frueh);
        } break;
        case 2: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_mittag);
        } break;
        case 3: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_abend);
        } break;
        case 4: {
            usedUhrType->show(w_morgen);
            usedUhrType->show(w_nacht);
        } break;
        }
        switch (wtemp_24) {
        case 30: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_dreissig);
            usedUhrType->show(w_grad);
        } break;
        case 25: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 20: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case 15: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 10: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case 5: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case 1: {
            usedUhrType->show(w_ueber);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -1: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_null);
            usedUhrType->show(w_grad);
        } break;
        case -5: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_grad);
        } break;
        case -10: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -15: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_zehn);
            usedUhrType->show(w_grad);
        } break;
        case -20: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        case -25: {
            usedUhrType->show(w_unter);
            usedUhrType->show(w_minus);
            usedUhrType->show(w_fuenf);
            usedUhrType->show(w_und);
            usedUhrType->show(w_zwanzig);
            usedUhrType->show(w_grad);
        } break;
        }
        switch (wwetter_24) {
        case 200:
            usedUhrType->show(w_gewitter);
            break;
        case 300:
            usedUhrType->show(w_regen);
            break;
        case 500:
            usedUhrType->show(w_regen);
            break;
        case 600:
            usedUhrType->show(w_schnee);
            break;
        case 700:
            usedUhrType->show(w_warnung);
            break;
        case 800:
            usedUhrType->show(w_klar);
            break;
        case 801:
            usedUhrType->show(w_wolken);
            break;
        }

    } break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::calcClockFace() {
    uint8_t rr, gg, bb, ww;

    if (_stunde == 23 && _minute == 59 && _sekunde >= 50) {
        countdownToMidnight();
    } else {
        clockSetClock();
    }

    // Helligkeitswert ermitteln
    if (_stunde < 6) {
        G.hh = G.h24;
    } else if (_stunde < 8) {
        G.hh = G.h6;
    } else if (_stunde < 12) {
        G.hh = G.h8;
    } else if (_stunde < 16) {
        G.hh = G.h12;
    } else if (_stunde < 18) {
        G.hh = G.h16;
    } else if (_stunde < 20) {
        G.hh = G.h18;
    } else if (_stunde < 22) {
        G.hh = G.h20;
    } else if (_stunde < 24) {
        G.hh = G.h22;
    }

    setBrightnessLdr(rr, gg, bb, ww, Background);

    // Hintergrund setzen
    for (uint16_t i = 0; i < usedUhrType->NUM_PIXELS(); i++) {
        ledSetPixel(rr, gg, bb, ww, i);
    }

    if (usedUhrType->hasWeatherLayout()) {
        clockShowWeather();
    }
}