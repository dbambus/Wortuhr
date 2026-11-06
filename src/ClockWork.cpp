#include "WordClockState.h"
#include "WordClockTypes/ClockType.hpp"

#include "ClockWork.h"

#include "ClockTypeRegistry.h"
#include "Config.h"
#include "Font.h"
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

void ClockWork::initHardwareButtons() {
    hardwareButtonController.begin(G.hardwarePins);
}

//------------------------------------------------------------------------------

void ClockWork::toggleHardwareButtonPower() {
    led.setState(!led.getState());

    requestHardwareButtonDisplayRefresh();
}

//------------------------------------------------------------------------------

void ClockWork::nextHardwareButtonMode() {
    static const uint8_t modes[] = {
        COMMAND_MODE_WORD_CLOCK,    COMMAND_MODE_SECONDS,
        COMMAND_MODE_SCROLLINGTEXT, COMMAND_MODE_RAINBOWCYCLE,
        COMMAND_MODE_RAINBOW,       COMMAND_MODE_FIRE,
        COMMAND_MODE_COLOR,         COMMAND_MODE_DIGITAL_CLOCK,
        COMMAND_MODE_SYMBOL};

    uint8_t nextMode = modes[0];
    for (uint8_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        if (G.prog == modes[i] ||
            (G.prog == COMMAND_IDLE && modes[i] == COMMAND_MODE_WORD_CLOCK)) {
            nextMode = modes[(i + 1) % (sizeof(modes) / sizeof(modes[0]))];
            break;
        }
    }

    G.prog = nextMode;
    G.progInit = true;
    parametersChanged = true;
    webStateChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::nextHardwareButtonTransition() {
    static const uint8_t transitions[] = {
        NO_TRANSITION, ROLL_UP,     ROLL_DOWN, SHIFT_LEFT, SHIFT_RIGHT, FADE,
        LASER,         MATRIX_RAIN, BALLS,     FIRE,       SNAKE,       RANDOM};

    uint8_t nextTransition = transitions[0];
    for (uint8_t i = 0; i < sizeof(transitions) / sizeof(transitions[0]); i++) {
        if (G.transitionType == transitions[i]) {
            nextTransition = transitions[(i + 1) % (sizeof(transitions) /
                                                    sizeof(transitions[0]))];
            break;
        }
    }

    G.transitionType = nextTransition;
    G.prog = COMMAND_MODE_TRANSITION;
    G.progInit = true;
    parametersChanged = true;
    webStateChanged = true;
}

//------------------------------------------------------------------------------

void ClockWork::increaseHardwareButtonBrightness() {
    float brightness = G.color[Foreground].B;
    if (brightness <= 0.0f) {
        brightness = 0.1f;
    } else if (brightness >= 1.0f) {
        brightness = 0.1f;
    } else {
        brightness += 0.1f;
        if (brightness > 1.0f) {
            brightness = 1.0f;
        }
    }

    G.color[Foreground].B = brightness;
    requestHardwareButtonDisplayRefresh();
}

//------------------------------------------------------------------------------

void ClockWork::nextHardwareButtonHue() {
    float hue = G.color[Foreground].H + (30.0f / 360.0f);
    if (hue >= 1.0f) {
        hue -= 1.0f;
    }
    G.color[Foreground].H = hue;
    requestHardwareButtonDisplayRefresh();
}

//------------------------------------------------------------------------------

void ClockWork::requestHardwareButtonDisplayRefresh() {
    parametersChanged = true;
    webStateChanged = true;
    if (G.prog == COMMAND_IDLE) {
        G.prog = COMMAND_MODE_WORD_CLOCK;
    }
}

//------------------------------------------------------------------------------

void ClockWork::handleHardwareButtonAction(HardwareButtonAction action) {
    switch (action) {
    case HardwareButtonAction::TogglePower:
        toggleHardwareButtonPower();
        break;
    case HardwareButtonAction::NextMode:
        nextHardwareButtonMode();
        break;
    case HardwareButtonAction::NextTransition:
        nextHardwareButtonTransition();
        break;
    case HardwareButtonAction::IncreaseBrightness:
        increaseHardwareButtonBrightness();
        break;
    case HardwareButtonAction::NextHue:
        nextHardwareButtonHue();
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

void ClockWork::loopHardwareButtons() {
    const HardwareButtonAction action = hardwareButtonController.loop();
    if (action != HardwareButtonAction::None) {
        handleHardwareButtonAction(action);
    }
}

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

namespace {

void sendJsonToClient(uint8_t clientNr, const JsonDocument &doc) {
    if (doc.overflowed()) {
        Serial.println("[ERROR] sendJsonToClient(): document capacity too "
                       "small, keys are missing");
    }

    const size_t payloadLength = measureJson(doc);

    String payload;
    payload.reserve(payloadLength + 1);
    serializeJson(doc, payload);

    if (payload.length() != payloadLength) {
        Serial.printf("[ERROR] sendJsonToClient(): out of memory for %u "
                      "bytes, payload dropped\n",
                      payloadLength + 1);
        return;
    }

    Serial.print("Sending Payload:");
    Serial.println(payload);

    webSocket.sendTXT(clientNr, payload);
}

//------------------------------------------------------------------------------

void broadcastJsonToClients(const JsonDocument &doc) {
    if (doc.overflowed()) {
        Serial.println("[ERROR] broadcastJsonToClients(): document capacity "
                       "too small, keys are missing");
    }

    String payload;
    serializeJson(doc, payload);
    webSocket.broadcastTXT(payload);
}

//------------------------------------------------------------------------------

void fillColorValues(JsonDocument &config) {
    config["command"] = "set";
    for (uint8_t i = 0; i < ColorPositionCount; i++) {
        char key[7];
        sprintf(key, "hsb%d%d", i, 0);
        config[key] = static_cast<uint16_t>(lroundf(G.color[i].H * 360));
        sprintf(key, "hsb%d%d", i, 1);
        config[key] = static_cast<uint8_t>(lroundf(G.color[i].S * 100));
        sprintf(key, "hsb%d%d", i, 2);
        config[key] = static_cast<uint8_t>(lroundf(G.color[i].B * 100));
    }
    config["effectBri"] = G.effectBri;
    config["effectSpeed"] = G.effectSpeed;
    config["colortype"] = G.colorType;
    config["hasSpecialWordHappyBirthday"] =
        usedClockType->hasSpecialWordHappyBirthday();
    config["hasSecondsFrame"] = usedClockType->hasSecondsFrame();
    config["prog"] = G.prog;
    config["bitmapSymbol"] = static_cast<uint8_t>(G.bitmapSymbol);
}

//------------------------------------------------------------------------------

void fillTransitionValues(JsonDocument &config) {
    config["command"] = "transition";
    config["transitionType"] = G.transitionType;
    config["transitionDuration"] = G.transitionDuration;
    config["transitionDemo"] = G.transitionDemo;
    config["colorize"] = G.colorize;
}

//------------------------------------------------------------------------------

void broadcastWebState() {
    if (!webStateChanged) {
        return;
    }
    webStateChanged = false;

    if (webSocket.connectedClients() == 0) {
        return;
    }

    DynamicJsonDocument transition(256);
    fillTransitionValues(transition);
    broadcastJsonToClients(transition);

    DynamicJsonDocument colors(768);
    fillColorValues(colors);
    broadcastJsonToClients(colors);
}

//------------------------------------------------------------------------------

void sendI2CScanResult(uint8_t clientNr, uint8_t sdaPin, uint8_t sclPin) {
    DynamicJsonDocument config(384);
    config["command"] = "i2c-scan";
    config["sdaPin"] = sdaPin;
    config["sclPin"] = sclPin;

    JsonArray addresses = config.createNestedArray("addresses");
    i2cBus::ScanResult result =
        i2cBus::scan(sdaPin, sclPin, G.i2cSdaPin, G.i2cSclPin);

    if (result.status == i2cBus::ScanStatus::Invalid) {
        config["status"] = "invalid";
        sendJsonToClient(clientNr, config);
        return;
    }

    if (result.status == i2cBus::ScanStatus::Disabled) {
        config["status"] = "disabled";
        sendJsonToClient(clientNr, config);
        return;
    }

    for (uint8_t i = 0; i < result.addressCount; i++) {
        char addressText[5] = {0};
        snprintf(addressText, sizeof(addressText), "0x%02X",
                 result.addresses[i]);
        addresses.add(addressText);
    }

    config["status"] = "ok";
    sendJsonToClient(clientNr, config);
}

} // namespace

//------------------------------------------------------------------------------
// Front Effect Functions
//------------------------------------------------------------------------------

void ClockWork::rainbow() {
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

void ClockWork::rainbowCycle() {
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

void ClockWork::fire() {
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

bool ClockWork::getStaticScrollingTextInfo(const char *buf,
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

void ClockWork::displayStaticScrollingText(const char *buf,
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

void ClockWork::scrollingText(const char *buf) {
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

void ClockWork::displaySymbols(BitmapSymbol symbolNum) {
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

//------------------------------------------------------------------------------

void ClockWork::countdownToMidnight() {
    Serial.printf("Count down: %d\n", 60 - _second);
    switch (_second) {
    case 50:
        if (!usedClockType->hasOnlyQuarterLayout()) {
            usedClockType->show(FrontWord::min_10);
        } else {
            usedClockType->show(FrontWord::hour_10);
        }
        break;
    case 51:
        usedClockType->show(FrontWord::hour_9);
        break;
    case 52:
        usedClockType->show(FrontWord::hour_8);
        break;
    case 53:
        usedClockType->show(FrontWord::hour_7);
        break;
    case 54:
        usedClockType->show(FrontWord::hour_6);
        break;
    case 55:
        if (!usedClockType->hasOnlyQuarterLayout()) {
            usedClockType->show(FrontWord::min_5);
        } else {
            usedClockType->show(FrontWord::hour_5);
        }
        break;
    case 56:
        usedClockType->show(FrontWord::hour_4);
        break;
    case 57:
        usedClockType->show(FrontWord::hour_3);
        break;
    case 58:
        usedClockType->show(FrontWord::hour_2);
        break;
    case 59:
        if (usedClockType->usedLang() == LanguageAbbreviation::DE) {
            usedClockType->show(FrontWord::eins);
        } else {
            usedClockType->show(FrontWord::hour_1);
        }
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------
// Boot Functions
//------------------------------------------------------------------------------

void ClockWork::initBootShowIp(const char *buf) {
    uint8_t StringLength = strlen(buf);
    StringLength = StringLength * 6; // Times 6, because thats the length of a
                                     // char in the 7x5 font plus spacing
    for (uint16_t i = 0; i <= StringLength; i++) {
        scrollingText(buf);
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
        countdownToMidnight();
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

void ClockWork::sendMqttValues() {
    DynamicJsonDocument config(512);
    config["command"] = "mqtt";
    config["MQTT_State"] = G.mqtt.state;
    config["MQTT_Port"] = G.mqtt.port;
    config["MQTT_Server"] = G.mqtt.serverAddress;
    config["MQTT_User"] = G.mqtt.user;

    // don't expose password:
    char passMasked[sizeof(G.mqtt.password) + 1] = {0};
    sensitive::maskPreservingSuffix(passMasked, G.mqtt.password);
    config["MQTT_Pass"] = passMasked;

    config["MQTT_ClientId"] = G.mqtt.clientId;
    config["MQTT_Topic"] = G.mqtt.topic;
    config["MQTT_Status"] = mqtt.status();

    sendJsonToClient(G.clientNr, config);
}

//------------------------------------------------------------------------------

void ClockWork::sendConfigValues() {
    DynamicJsonDocument config(1280);
    config["command"] = "config";
    config["ssid"] = network.getSSID();
    config["timeserver"] = G.timeserver;
    config["timezone"] = G.timezone;
    config["hostname"] = G.hostname;
    config["scrollingText"] = G.scrollingText;
    config["h6"] = G.h6;
    config["h8"] = G.h8;
    config["h12"] = G.h12;
    config["h16"] = G.h16;
    config["h18"] = G.h18;
    config["h20"] = G.h20;
    config["h22"] = G.h22;
    config["h24"] = G.h24;
    for (uint8_t i = 0;
         i < sizeof(G.languageVariant) / sizeof(G.languageVariant[0]); i++) {
        char key[11];
        sprintf(key, "langVar%d", i);
        config[key] = static_cast<uint8_t>(G.languageVariant[i]);
    }
    for (uint8_t i = 0;
         i < sizeof(G.layoutVariant) / sizeof(G.layoutVariant[0]); i++) {
        char key[11];
        sprintf(key, "layVar%d", i);
        config[key] = static_cast<uint8_t>(G.layoutVariant[i]);
    }
    config["effectBri"] = G.effectBri;
    config["itIsVariant"] = static_cast<uint8_t>(G.itIsVariant);
    config["secondVariant"] = static_cast<uint8_t>(G.secondVariant);
    config["minuteVariant"] = static_cast<uint8_t>(G.minuteVariant);
    config["supportedMinuteVariants"] =
        usedClockType->supportedMinuteVariantMask();
    config["cityid"] = G.openWeatherMap.cityId;
    char apiKeyMasked[sizeof(G.openWeatherMap.apiKey) + 1] = {0};
    sensitive::maskPreservingSuffix(apiKeyMasked, G.openWeatherMap.apiKey);
    config["apiKey"] = apiKeyMasked;
    config["colortype"] = G.colorType;
    config["buildtype"] = static_cast<uint8_t>(G.buildTypeDef);
    config["wType"] = static_cast<uint8_t>(G.whiteType);
    config["clockTypeDef"] = G.clockTypeDef;
    config["bitmapSymbol"] = static_cast<uint8_t>(G.bitmapSymbol);
    config["bootLedBlink"] = G.bootLedBlink;
    config["bootLedSweep"] = G.bootLedSweep;
    config["bootShowWifi"] = G.bootShowWifi;
    config["bootShowIP"] = G.bootShowIP;
    config["ledPin"] = G.hardwarePins.led;
    config["powerButtonPin"] = G.hardwarePins.powerButton;
    config["modeButtonPin"] = G.hardwarePins.modeButton;
    config["speedButtonPin"] = G.hardwarePins.speedButton;
    config["i2cSdaPin"] = G.i2cSdaPin;
    config["i2cSclPin"] = G.i2cSclPin;
    config["autoBrightEnabled"] = G.autoBrightEnabled;
    config["isRomanLanguage"] = usedClockType->isRomanLanguage();
    config["hasDreiviertel"] = usedClockType->hasDreiviertel();
    config["hasTwenty"] = usedClockType->hasTwenty();
    config["hasWeatherLayout"] = usedClockType->hasWeatherLayout();
    config["hasSecondsFrame"] = usedClockType->hasSecondsFrame();
    config["supportsSecondsFrame"] = usedClockType->supportsSecondsFrame();
    config["secondsFrameLedCount"] = G.secondsFrameLedCount;
    config["hasMinuteInWords"] = usedClockType->hasMinuteInWords();
    config["hasSpecialWordHappyBirthday"] =
        usedClockType->hasSpecialWordHappyBirthday();
    config["numOfRows"] = usedClockType->rowsWordMatrix();
    config["fireCooling"] = G.fireCooling;
    config["fireSparking"] = G.fireSparking;

    sendJsonToClient(G.clientNr, config);
}

//------------------------------------------------------------------------------

void ClockWork::sendBirthdays() {
    DynamicJsonDocument config(256);
    config["command"] = "birthdays";
    config["hasSpecialWordHappyBirthday"] =
        usedClockType->hasSpecialWordHappyBirthday();
    for (uint8_t i = 0; i < MAX_BIRTHDAY_COUNT; i++) {
        char dateString[10];
        char key[16];
        snprintf(key, sizeof(key), "birthdayDate%d", i);
        snprintf(dateString, sizeof(dateString), "%02u-%02u",
                 G.birthday[i].month, G.birthday[i].day);
        config[key] = dateString;
    }

    sendJsonToClient(G.clientNr, config);
}

//------------------------------------------------------------------------------

void ClockWork::sendColorValues() {
    DynamicJsonDocument config(768);
    fillColorValues(config);
    sendJsonToClient(G.clientNr, config);
}

//------------------------------------------------------------------------------

void ClockWork::sendAutoBrightValues() {
    DynamicJsonDocument config(256);
    config["command"] = "autoBright";
    config["autoBrightEnabled"] = G.autoBrightEnabled;
    config["autoBrightMin"] = G.autoBrightMin;
    config["autoBrightMax"] = G.autoBrightMax;
    config["autoBrightPeak"] = G.autoBrightPeak;
    config["autoBrightSensor"] = autoBrightness.lux();
    config["autoBrightGain"] = (uint8_t)ledGain;
    config["autoBrightSource"] = autoBrightness.usesBH1750() ? "BH1750" : "LDR";

    sendJsonToClient(G.clientNr, config);
}

//------------------------------------------------------------------------------

void ClockWork::sendTransitionValues() {
    DynamicJsonDocument config(256);
    fillTransitionValues(config);
    sendJsonToClient(G.clientNr, config);
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

void ClockWork::sendI2CScan() {
    sendI2CScanResult(G.clientNr, i2cScanSdaPin, i2cScanSclPin);
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
        sendMqttValues();
        break;
    case COMMAND_REQUEST_CONFIG_VALUES:
        sendConfigValues();
        break;
    case COMMAND_REQUEST_BIRTHDAYS:
        sendBirthdays();
        break;
    case COMMAND_REQUEST_I2C_SCAN:
        sendI2CScan();
        break;
    case COMMAND_REQUEST_COLOR_VALUES:
        sendColorValues();
        break;
    case COMMAND_REQUEST_AUTO_BRIGHT:
        sendAutoBrightValues();
        break;
    case COMMAND_REQUEST_TRANSITION:
        sendTransitionValues();
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
        scrollingText(G.scrollingText);
        break;
    case COMMAND_MODE_RAINBOWCYCLE:
        rainbowCycle();
        break;
    case COMMAND_MODE_RAINBOW:
        rainbow();
        break;
    case COMMAND_MODE_FIRE:
        fire();
        break;
    case COMMAND_MODE_SYMBOL:
        displaySymbols(G.bitmapSymbol);
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
    loopHardwareButtons();

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

    broadcastWebState();

    runActiveMode();

    if (countMillisSpeed > 10000) {
        countMillisSpeed = 0;
    }
}
