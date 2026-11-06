#pragma once

#include "ClockFace.h"
#include "HardwareButtonController.h"
#include <NeoPixelBus.h>

class ClockWork {
private:
    uint16_t countMillisSpeed = 0;
    uint32_t previousMillis = 0;
    uint32_t lux = 0;
    HardwareButtonController hardwareButtonController;
    ClockFace clockFace;

private:
    //------------------------------------------------------------------------------
    // Helper Functions
    //------------------------------------------------------------------------------
    void loopAutoBrightLogic();
    void loopHardwareButtons();
    void handleHardwareButtonAction(HardwareButtonAction action);
    void toggleHardwareButtonPower();
    void nextHardwareButtonMode();
    void nextHardwareButtonTransition();
    void increaseHardwareButtonBrightness();
    void nextHardwareButtonHue();
    void requestHardwareButtonDisplayRefresh();
    uint32_t allColumnsMask();
    void reallocateSecondsFrame();

    //------------------------------------------------------------------------------
    // Front Effect Functions
    //------------------------------------------------------------------------------
    struct StaticScrollingText {
        uint8_t start = 0;
        uint8_t len = 0;
        uint8_t width = 0;
        uint8_t height = 0;
        uint8_t totalWidth = 0;
        bool onlyDigits = true;
    };

    void rainbow();
    void rainbowCycle();
    void fire();
    bool getStaticScrollingTextInfo(const char *buf, StaticScrollingText &info);
    void displayStaticScrollingText(const char *buf,
                                    const StaticScrollingText &info);
    void scrollingText(const char *buf);
    void displaySymbols(BitmapSymbol bitmapSymbol);
    void countdownToMidnight();

    //------------------------------------------------------------------------------
    // Loop Helper Functions
    //------------------------------------------------------------------------------

    WordclockChanges changesInClockface();
    void calcClockface();
    void clearClockByProgInit();
    void loopEverySecond(struct tm &tm);
    void loopEveryMinute();
    void handleConfigCommand();
    void sendMqttValues();
    void sendConfigValues();
    void sendBirthdays();
    void sendColorValues();
    void sendAutoBrightValues();
    void sendTransitionValues();
    void applyHardwarePins();
    void applyClockType();
    void applySecondsFrame();
    void runActiveMode();
    void showSecondsMode();
    void showDigitalClockMode();
    void runEffectMode();
    void showColorMode();
    void persistTransitionMode();
    void showWordClockMode();

public:
    ~ClockWork() = default;

    //------------------------------------------------------------------------------
    // Helper Functions
    //------------------------------------------------------------------------------
    void initLedStrip(uint8_t num);
    void initHardwareButtons();
    uint32_t getLuxValue() const { return lux; }

    //------------------------------------------------------------------------------
    // Minute Functions
    //------------------------------------------------------------------------------
    void normalizeMinuteVariant();
    void checkForValidLanguageVariant();

    //------------------------------------------------------------------------------
    // Boot Functions
    //------------------------------------------------------------------------------
    void initBootLed();
    void initBootLedBlink();
    void initBootLedSweep(uint32_t delay);
    void initBootShowIp(const char *buf);

    //------------------------------------------------------------------------------
    // Loop Functions
    //------------------------------------------------------------------------------
    void loop(struct tm &tm);
};
