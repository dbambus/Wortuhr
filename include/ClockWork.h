#pragma once

#include "AutoBrightness.h"
#include "ClockFace.h"
#include "EffectModes.h"
#include "HardwareButtons.h"
#include <NeoPixelBus.h>

class ClockWork {
private:
    uint16_t countMillisSpeed = 0;
    uint32_t previousMillis = 0;
    HardwareButtons hardwareButtons;
    EffectModes effectModes;
    ClockFace clockFace;
    AutoBrightness autoBrightness;

private:
    //------------------------------------------------------------------------------
    // Helper Functions
    //------------------------------------------------------------------------------
    uint32_t allColumnsMask();
    void reallocateSecondsFrame();

    //------------------------------------------------------------------------------
    // Loop Helper Functions
    //------------------------------------------------------------------------------

    WordclockChanges changesInClockface();
    void calcClockface();
    void clearClockByProgInit();
    void loopEverySecond(struct tm &tm);
    void loopEveryMinute();
    void handleConfigCommand();
    void restart();
    void saveSettings();
    void saveAndRedraw();
    void saveAndRestartMode();
    void applyTimezoneSetting();
    void applyBuildType();
    void applyMinuteSetting();
    void applyBrightness();
    void applyMqttSettings();
    void sendMqttDiscovery();
    void applyColorType();
    void saveFireSettings();
    void saveEffectSpeed();
    void applyHostname();
    void disableWifi();
    void resetWifi();
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
    void initAutoBrightness();
    const AutoBrightness &getAutoBrightness() const { return autoBrightness; }

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
