#include "WordClockState.h"
#include "WordClockTypes/ClockType.hpp"

#include "ClockWork.h"

#include "ClockTypeRegistry.h"
#include "Config.h"
#include "Frame.h"
#include "I2CBus.h"
#include "Led.h"
#include "LedStripInterface.h"
#include "Mqtt.h"
#include "NeoMultiFeature.hpp"
#include "NeoPixelBusLedStripAdapter.h"
#include "Network.h"
#include "OpenWeatherMap.h"
#include "SensitiveData.h"
#include "WebPageAdapter.h"
#include "WebStatus.h"
#include "WordClock.h"
#include "math.h"
#include <Arduino.h>
#include <ArduinoJson.h>

namespace eeprom {
void write();
}

extern ClockType *usedClockType;
extern Led led;
extern LedStripInterface *activeLedStrip;
extern SecondsFrame *secondsFrame;
extern Mqtt mqtt;
extern Network network;

#include "Render/ColorStage.h"
#include "Render/RenderPipeline.h"
#include "Transitions/Transition.h"

extern ColorStage colorStage;
extern RenderPipeline renderPipeline;

namespace {
uint8_t activeLedPin = UINT8_MAX;
uint8_t activeLedColorType = UINT8_MAX;
uint16_t activeLedCount = 0;
} // namespace

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

void ClockWork::initAutoBrightness() { autoBrightness.begin(); }

//------------------------------------------------------------------------------

void ClockWork::initHardwareButtons() { hardwareButtons.begin(G.hardwarePins); }

//------------------------------------------------------------------------------

void ClockWork::reallocateSecondsFrame() {
    delete secondsFrame;
    secondsFrame = nullptr;

    if (usedClockType->numPixelsFrameMatrix() != 0) {
        secondsFrame = new SecondsFrame(usedClockType->numPixelsFrameMatrix());
        G.progInit = true;
    }
}

//------------------------------------------------------------------------------

void ClockWork::initLedStrip(uint8_t num) {
    NeoMultiFeature::setColortype(num);
    const uint16_t ledCount = usedClockType->numPixelsOnStrip();

    if (activeLedPin != G.hardwarePins.led || activeLedColorType != num ||
        activeLedCount != ledCount) {
        deleteActiveLedStrip();
        activeLedPin = G.hardwarePins.led;
        activeLedColorType = num;
        activeLedCount = ledCount;
    }

    if (activeLedStrip != nullptr) {
        return;
    }

#ifdef ESP8266
    if (num == Grbw) {
        if (G.hardwarePins.led == LED_PIN) {
            activeLedStrip =
                new RgbwLedStripAdapter<Neo800KbpsMethod>(ledCount);
        } else {
            activeLedStrip =
                new RgbwLedStripAdapter<NeoEsp8266BitBangWs2812xMethod>(
                    ledCount, G.hardwarePins.led);
        }
    } else if (G.hardwarePins.led == LED_PIN) {
        activeLedStrip = new RgbLedStripAdapter<Neo800KbpsMethod>(ledCount);
    } else {
        activeLedStrip = new RgbLedStripAdapter<NeoEsp8266BitBangWs2812xMethod>(
            ledCount, G.hardwarePins.led);
    }
#else
#if defined(ESP32)
    pinMode(G.hardwarePins.led, OUTPUT);
#endif
    if (num == Grbw) {
        activeLedStrip = new RgbwLedStripAdapter<NeoEsp32Rmt0Ws2812xMethod>(
            ledCount, G.hardwarePins.led);
    } else {
        activeLedStrip = new RgbLedStripAdapter<NeoEsp32Rmt0Ws2812xMethod>(
            ledCount, G.hardwarePins.led);
    }
#endif
}

//------------------------------------------------------------------------------

uint32_t ClockWork::allColumnsMask() {
    return (1U << usedClockType->colsWordMatrix()) - 1;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Boot Functions
//------------------------------------------------------------------------------

void ClockWork::initBootShowIp(const char *buf) {
    uint8_t StringLength = strlen(buf);
    StringLength = StringLength * 6; // Times 6, because thats the length of a
                                     // char in the 7x5 font plus spacing
    for (uint16_t i = 0; i <= StringLength; i++) {
        effectModes.scrollingText(buf);
        delay(200);
    }
}

//------------------------------------------------------------------------------

void ClockWork::initBootLedBlink() {
    for (uint8_t row = 0; row < usedClockType->rowsWordMatrix(); row++) {
        frontMatrix[row] ^= allColumnsMask();
    }
    led.setbyFrontMatrix(Foreground);
    led.show();
}

//------------------------------------------------------------------------------

void ClockWork::initBootLedSweep(uint32_t delay) { led.setSingle(delay); }

//------------------------------------------------------------------------------

void ClockWork::initBootLed() {
    led.clear();
    led.show();
}

//------------------------------------------------------------------------------
// Minute Functions
//------------------------------------------------------------------------------

void ClockWork::checkForValidLanguageVariant() {
    G.languageVariant[ItIs15] = false;
    G.languageVariant[ItIs20] = usedClockType->hasTwenty();
    G.languageVariant[ItIs40] = usedClockType->hasTwenty();
    G.languageVariant[ItIs45] = usedClockType->hasDreiviertel();
}

//------------------------------------------------------------------------------

void ClockWork::normalizeMinuteVariant() {
    if (!usedClockType->supportsMinuteVariant(G.minuteVariant)) {
        G.minuteVariant = MinuteVariant::Off;
    }

    G.minuteLedCount = minuteLedCountFor(G.minuteVariant, G.minuteLedCount);
}

// Loop Helper Functions
//------------------------------------------------------------------------------

WordclockChanges ClockWork::changesInClockface() {
    for (uint16_t i = 0; i < MAX_ROW_SIZE; i++) {
        if (frontMatrix[i] != lastFrontMatrix[i]) {
            return WordclockChanges::Words;
        }
    }
    if (parametersChanged) {
        parametersChanged = false;
        return WordclockChanges::Parameters;
    } else if (lastMinuteArray != minuteArray) {
        return WordclockChanges::Minute;
    }
    return WordclockChanges::Null;
}

//------------------------------------------------------------------------------

void ClockWork::calcClockface() {
    led.resetFrontMatrixBuffer();

    if (_hour == 23 && _minute == 59 && _second >= 50) {
        clockFace.showCountdownToMidnight(_second);
    } else {
        clockFace.show(_hour, _minute);
    }

    if (usedClockType->hasWeatherLayout()) {
        weather.calcWeatherClockface();
    }
}

//------------------------------------------------------------------------------

void ClockWork::clearClockByProgInit() {
    if (G.progInit) {
        G.progInit = false;
        led.clear();
        led.show();
    }
}

//------------------------------------------------------------------------------
// Loop Functions
//------------------------------------------------------------------------------

void ClockWork::loopEverySecond(struct tm &tm) {
#if GENERAL_VERBOSE
    char currentTime[80];
    strftime(currentTime, sizeof(currentTime), "%F %T (%z)\n", &tm);
    Serial.printf(currentTime);
#endif

    if (usedClockType->hasWeatherLayout()) {
        weather.loop();
    }

    autoBrightness.loop();

    if (G.prog == COMMAND_IDLE && G.conf == COMMAND_IDLE) {
        led.clear();
        G.prog = COMMAND_MODE_WORD_CLOCK;
    }
}

//------------------------------------------------------------------------------

void ClockWork::loopEveryMinute() {
    if (colorChangedByWebsite) {
        eeprom::write();
        colorChangedByWebsite = false;
        Serial.println("Saved Color");
    }

    sendMQTTUpdate();
}

//------------------------------------------------------------------------------

void ClockWork::applyHardwarePins() {
    if (!hardwarePinsAreValid()) {
        Serial.println("Invalid hardware pin configuration, restoring "
                       "defaults");
        setDefaultHardwarePins();
        i2cBus::setDefaultPins(G.i2cSdaPin, G.i2cSclPin);
    }

    Serial.printf("Hardware LED pin: GPIO%u\n", G.hardwarePins.led);
    Serial.printf("Hardware power button pin: GPIO%u\n",
                  G.hardwarePins.powerButton);
    Serial.printf("Hardware mode button pin: GPIO%u\n",
                  G.hardwarePins.modeButton);
    Serial.printf("Hardware speed button pin: GPIO%u\n",
                  G.hardwarePins.speedButton);
    Serial.printf("I2C SDA pin: GPIO%u\n", G.i2cSdaPin);
    Serial.printf("I2C SCL pin: GPIO%u\n", G.i2cSclPin);

    eeprom::write();
    i2cBus::begin(G.i2cSdaPin, G.i2cSclPin);
    initHardwareButtons();
    led.clear();
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::applyClockType() {
    if (!isValidClockTypeDef(G.clockTypeDef)) {
        Serial.printf("Ignoring invalid ClockType: %u\n", G.clockTypeDef);
        return;
    }

    led.clear();
    led.show();
    delay(10);
    Serial.printf("ClockType: %u\n", G.clockTypeDef);

    usedClockType = clockTypeFor(G.clockTypeDef);
    normalizeMinuteVariant();

    checkForValidLanguageVariant();
    eeprom::write();

    reallocateSecondsFrame();

    renderPipeline.resize(usedClockType->rowsWordMatrix(),
                          usedClockType->colsWordMatrix());

    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::applySecondsFrame() {
    // led.clear() must run while G.secondsFrameLedCount still holds the
    // OLD count, so a shrink clears the LEDs that are about to fall out
    // of range - clearFrame() loops on the CURRENT count, and updating
    // G.secondsFrameLedCount first would leave those LEDs stuck on.
    led.clear();
    led.show();
    delay(10);

    G.secondsFrameLedCount = G.param1;
    eeprom::write();
    Serial.printf("Seconds frame LED count: %u\n", G.secondsFrameLedCount);

    reallocateSecondsFrame();

    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::restart() {
    delay(500);
    ESP.restart();
}

//------------------------------------------------------------------------------

void ClockWork::saveSettings() {
    eeprom::write();
    delay(100);
}

//------------------------------------------------------------------------------

void ClockWork::applyTimezoneSetting() {
    applyTimezone();
    eeprom::write();
    parametersChanged = true;
    delay(100);
}

//------------------------------------------------------------------------------

void ClockWork::applyBuildType() {
    led.clear();

    // G.param1 sets new buildtype
    Serial.printf("Clock Buildtype: %u\n", G.param1);

    G.buildTypeDef = static_cast<BuildTypeDef>(G.param1);
    eeprom::write();
    led.clear();
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::applyMinuteSetting() {
    led.clear();
    normalizeMinuteVariant();
    eeprom::write();
    memset(frameArray, false, sizeof(frameArray));
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::applyBrightness() {
    if (G.param1) {
        eeprom::write();
    }
    led.clear();
    memset(frameArray, false, sizeof(frameArray));
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::saveAndRedraw() {
    eeprom::write();
    led.clear();
    memset(frameArray, false, sizeof(frameArray));
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::saveAndRestartMode() {
    eeprom::write();
    led.clear();
    G.progInit = true;
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::applyMqttSettings() {
    if (G.mqtt.state && G.progInit) {
        mqtt.init();
        G.progInit = false;
    }
    delay(100);

    eeprom::write();
    mqtt.notifyStatus();
}

//------------------------------------------------------------------------------

void ClockWork::sendMqttDiscovery() { mqtt.sendDiscovery(); }

//------------------------------------------------------------------------------

void ClockWork::applyColorType() {
    // G.param1 sets new colorType
    Serial.printf("LED Colortype: %u\n", G.param1);

    G.colorType = G.param1;
    eeprom::write();

    clearClockByProgInit();
    parametersChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::saveFireSettings() {
    if (G.param1) {
        eeprom::write();
        Serial.printf("Fire: cooling %u, sparking %u\n", G.fireCooling,
                      G.fireSparking);
    }
}

//------------------------------------------------------------------------------

void ClockWork::saveEffectSpeed() {
    if (G.param1) {
        eeprom::write();
        Serial.printf("Effect speed: %u\n", G.effectSpeed);
    }
}

//------------------------------------------------------------------------------

void ClockWork::applyHostname() {
    Serial.print("Hostname: ");
    Serial.println(G.hostname);
    eeprom::write();
    network.changeHostname(G.hostname);
}

//------------------------------------------------------------------------------

void ClockWork::disableWifi() {
    eeprom::write();
    delay(100);
    Serial.println("Conf: WLAN off");
    network.disable();
}

//------------------------------------------------------------------------------

void ClockWork::resetWifi() {
    Serial.println("Conf: new Wifi Config");
    network.resetSettings();
}

//------------------------------------------------------------------------------

void ClockWork::handleConfigCommand() {
    switch (G.conf) {
    case COMMAND_RESET:
        restart();
        break;
    case COMMAND_REQUEST_MQTT_VALUES:
        webStatus::sendMqttValues(G.clientNr);
        break;
    case COMMAND_REQUEST_CONFIG_VALUES:
        webStatus::sendConfigValues(G.clientNr);
        break;
    case COMMAND_REQUEST_BIRTHDAYS:
        webStatus::sendBirthdays(G.clientNr);
        break;
    case COMMAND_REQUEST_I2C_SCAN:
        webStatus::sendI2CScan(G.clientNr);
        break;
    case COMMAND_REQUEST_COLOR_VALUES:
        webStatus::sendColorValues(G.clientNr);
        break;
    case COMMAND_REQUEST_AUTO_BRIGHT:
        webStatus::sendAutoBrightValues(G.clientNr, autoBrightness);
        break;
    case COMMAND_REQUEST_TRANSITION:
        webStatus::sendTransitionValues(G.clientNr);
        break;
    case COMMAND_SET_BIRTHDAYS:
    case COMMAND_SET_TIME:
    case COMMAND_SET_INITIAL_VALUES:
    case COMMAND_SET_WEATHER_DATA:
    case COMMAND_SET_SCROLLINGTEXT:
    case COMMAND_SET_TIMESERVER:
    case COMMAND_SET_SYMBOL:
    case COMMAND_SET_BOOT:
        saveSettings();
        break;
    case COMMAND_SET_AUTO_BRIGHT:
    case COMMAND_SET_WHITETYPE:
    case COMMAND_SET_TIME_MANUAL:
    case COMMAND_SET_LAYOUT_VARIANT:
        saveAndRedraw();
        break;
    case COMMAND_SET_COLORIZE:
    case COMMAND_SET_LANGUAGE_VARIANT:
    case COMMAND_SET_IT_IS_VARIANT:
    case COMMAND_SET_SETTING_SECOND:
        saveAndRestartMode();
        break;
    case COMMAND_SET_TIMEZONE:
        applyTimezoneSetting();
        break;
    case COMMAND_SET_BUILDTYPE:
        applyBuildType();
        break;
    case COMMAND_SET_MINUTE:
        applyMinuteSetting();
        break;
    case COMMAND_SET_BRIGHTNESS:
        applyBrightness();
        break;
    case COMMAND_SET_MQTT:
        applyMqttSettings();
        break;
    case COMMAND_SET_MQTT_HA_DISCOVERY:
        sendMqttDiscovery();
        break;
    case COMMAND_SET_HARDWARE_PINS:
        applyHardwarePins();
        break;
    case COMMAND_SET_COLORTYPE:
        applyColorType();
        break;
    case COMMAND_SET_CLOCK_TYPE:
        applyClockType();
        break;
    case COMMAND_SET_SECONDS_FRAME:
        applySecondsFrame();
        break;
    case COMMAND_SET_FIRE:
        saveFireSettings();
        break;
    case COMMAND_SPEED:
        saveEffectSpeed();
        break;
    case COMMAND_SET_HOSTNAME:
        applyHostname();
        break;
    case COMMAND_SET_WIFI_DISABLED:
        disableWifi();
        break;
    case COMMAND_SET_WIFI_AND_RESTART:
        resetWifi();
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::showSecondsMode() {
    // 0xFF marks "nothing drawn yet" and cannot collide with a second.
    static uint8_t lastShownSecond = 0xFF;

    if (G.progInit) {
        clearClockByProgInit();
        lastShownSecond = 0xFF;
    }

    // This runs on every loop() iteration. Without this guard the LED
    // strip is rewritten thousands of times per second, which starves the
    // network stack -- the display only ever changes once per second.
    if (lastShownSecond == _second && !parametersChanged) {
        return;
    }
    lastShownSecond = _second;
    parametersChanged = false;

    led.showNumbers('0' + _second / 10, '0' + _second % 10);
}

//------------------------------------------------------------------------------

void ClockWork::showDigitalClockMode() {
    static uint8_t lastShownSecond = 0xFF;

    if (G.progInit) {
        clearClockByProgInit();
        lastShownSecond = 0xFF;
    }

    if (lastShownSecond == _second && !parametersChanged) {
        return;
    }
    lastShownSecond = _second;
    parametersChanged = false;

    led.clear();
    led.showDigitalClock(_minute % 10, _minute / 10, _hour % 10, _hour / 10);
}

//------------------------------------------------------------------------------

void ClockWork::runEffectMode() {
    const uint16_t effectInterval = effectSpeedIntervalMillis(G.effectSpeed);
    if (G.progInit) {
        countMillisSpeed = effectInterval;
        clearClockByProgInit();
    }

    if (countMillisSpeed < effectInterval) {
        return;
    }

    switch (G.prog) {
    case COMMAND_MODE_SCROLLINGTEXT:
        effectModes.scrollingText(G.scrollingText);
        break;
    case COMMAND_MODE_RAINBOWCYCLE:
        effectModes.rainbowCycle();
        break;
    case COMMAND_MODE_RAINBOW:
        effectModes.rainbow();
        break;
    case COMMAND_MODE_FIRE:
        effectModes.fire();
        break;
    case COMMAND_MODE_SYMBOL:
        effectModes.displaySymbols(G.bitmapSymbol);
        break;
    default:
        break;
    }
    countMillisSpeed = 0;
}

//------------------------------------------------------------------------------

void ClockWork::showColorMode() {
    if (G.progInit) {
        clearClockByProgInit();
        for (uint8_t row = 0; row < usedClockType->rowsWordMatrix(); row++) {
            frontMatrix[row] = allColumnsMask();
        }
    }

    if (parametersChanged) {
        led.setbyFrontMatrix(Foreground, false);
        led.show();
        parametersChanged = false;
    }
}

//------------------------------------------------------------------------------

void ClockWork::persistTransitionMode() {
    if (G.progInit) {
        G.progInit = false;
        eeprom::write();
        delay(100);
    }
}

//------------------------------------------------------------------------------

void ClockWork::showWordClockMode() {
    clearClockByProgInit();
    calcClockface();

    switch (changesInClockface()) {
    case WordclockChanges::Minute:
        lastMinuteArray = minuteArray;
        memcpy(&lastFrontMatrix, &frontMatrix, sizeof lastFrontMatrix);
        led.set(WordclockChanges::Minute);
        break;
    case WordclockChanges::Words:
        lastMinuteArray = minuteArray;
        memcpy(&lastFrontMatrix, &frontMatrix, sizeof lastFrontMatrix);
        led.set(WordclockChanges::Words);
        break;
    case WordclockChanges::Parameters:
        led.set(WordclockChanges::Parameters);
        break;
    default:
        break;
    }
    G.prog = COMMAND_IDLE;
}

//------------------------------------------------------------------------------

void ClockWork::runActiveMode() {
    switch (G.prog) {
    case COMMAND_MODE_SECONDS:
        showSecondsMode();
        break;

    case COMMAND_MODE_DIGITAL_CLOCK:
        showDigitalClockMode();
        break;

    case COMMAND_MODE_SCROLLINGTEXT:
    case COMMAND_MODE_RAINBOWCYCLE:
    case COMMAND_MODE_RAINBOW:
    case COMMAND_MODE_FIRE:
    case COMMAND_MODE_SYMBOL:
        runEffectMode();
        break;

    case COMMAND_MODE_COLOR:
        showColorMode();
        break;

    case COMMAND_MODE_TRANSITION:
        persistTransitionMode();
        showWordClockMode();
        break;

    case COMMAND_MODE_WORD_CLOCK:
        showWordClockMode();
        break;

    default:
        break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::loop(struct tm &tm) {
    unsigned long currentMillis = millis();
    countMillisSpeed += currentMillis - previousMillis;
    previousMillis = currentMillis;
    hardwareButtons.loop();

    // Faster runtime for demo
    renderPipeline.demoMode(_hour, _minute, _second);

    if (lastSecond != _second) {
        loopEverySecond(tm);
        lastSecond = _second;
    }

    if (lastMinute != _minute) {
        lastMinute = _minute;
        loopEveryMinute();
    }

    const bool setCommandPending =
        G.conf != COMMAND_IDLE && G.conf < PLACEHOLDER_MAX_SET;

    handleConfigCommand();

    if (setCommandPending) {
        initLedStrip(G.colorType);
    }

    G.conf = COMMAND_IDLE;

    webStatus::broadcastChanges();

    runActiveMode();

    if (countMillisSpeed > 10000) {
        countMillisSpeed = 0;
    }
}
