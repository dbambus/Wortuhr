#include "WebStatus.h"

#include "I2CBus.h"
#include "Mqtt.h"
#include "Network.h"
#include "SensitiveData.h"
#include "WebPageAdapter.h"
#include "WordClockState.h"
#include "WordClockTypes/ClockType.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>

extern ClockType *usedClockType;
extern Mqtt mqtt;
extern Network network;

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

namespace webStatus {

void sendMqttValues(uint8_t clientNr) {
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

    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendConfigValues(uint8_t clientNr) {
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

    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendBirthdays(uint8_t clientNr) {
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

    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendColorValues(uint8_t clientNr) {
    DynamicJsonDocument config(768);
    fillColorValues(config);
    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendAutoBrightValues(uint8_t clientNr,
                          const AutoBrightness &autoBrightness) {
    DynamicJsonDocument config(256);
    config["command"] = "autoBright";
    config["autoBrightEnabled"] = G.autoBrightEnabled;
    config["autoBrightMin"] = G.autoBrightMin;
    config["autoBrightMax"] = G.autoBrightMax;
    config["autoBrightPeak"] = G.autoBrightPeak;
    config["autoBrightSensor"] = autoBrightness.lux();
    config["autoBrightGain"] = (uint8_t)ledGain;
    config["autoBrightSource"] = autoBrightness.usesBH1750() ? "BH1750" : "LDR";

    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendTransitionValues(uint8_t clientNr) {
    DynamicJsonDocument config(256);
    fillTransitionValues(config);
    sendJsonToClient(clientNr, config);
}

//------------------------------------------------------------------------------

void sendI2CScan(uint8_t clientNr) {
    sendI2CScanResult(clientNr, i2cScanSdaPin, i2cScanSclPin);
}

//------------------------------------------------------------------------------

void broadcastChanges() {
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

} // namespace webStatus
