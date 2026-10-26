#include "SensitiveData.h"
#include "TelnetLog.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

namespace eeprom {

/*
 * Legacy raw-config storage.
 *
 * The public read()/write() API intentionally remains unchanged for now: the
 * complete global config struct G is persisted byte-for-byte. Future storage
 * work should migrate this to a versioned config format or a packed config
 * struct with explicit migration logic.
 */

namespace detail {

template <size_t sourceSize>
void printSafeString(const char *label, const char (&source)[sourceSize]) {
    char buffer[sourceSize + 1] = {0};
    sensitive::copyBoundedString(buffer, source);
    logger.printf("%s%s\n", label, buffer);
}

void printMaskedPassword() {
    char passMasked[sizeof(G.mqtt.password) + 1] = {0};
    sensitive::maskPreservingSuffix(passMasked, G.mqtt.password);
    logger.printf("MQTT_Pass (masked): %s\n", passMasked);
}

void printMaskedOpenWeatherMapApiKey() {
    char apiKeyMasked[sizeof(G.openWeatherMap.apikey) + 1] = {0};
    sensitive::maskPreservingSuffix(apiKeyMasked, G.openWeatherMap.apikey);
    logger.printf("OWM_apikey (masked): %s\n", apiKeyMasked);
}

void printConfig() {
    logger.printf("Version   : %s\n", VERSION);
    logger.printf("Sernr     : %u\n", G.sernr);
    logger.printf("Programm  : %u\n", G.prog);
    logger.printf("Conf      : %u\n", G.conf);
    logger.printf("FgCol.H   : %f\n", G.color[Foreground].H);
    logger.printf("FgCol.S   : %f\n", G.color[Foreground].S);
    logger.printf("FgCol.V   : %f\n", G.color[Foreground].B);
    logger.printf("BgCol.H   : %f\n", G.color[Background].H);
    logger.printf("BgCol.S   : %f\n", G.color[Background].S);
    logger.printf("BgCol.V   : %f\n", G.color[Background].B);
    logger.printf("FrCol.H   : %f\n", G.color[Frame].H);
    logger.printf("FrCol.S   : %f\n", G.color[Frame].S);
    logger.printf("FrCol.V   : %f\n", G.color[Frame].B);
    printSafeString("Zeitserver: ", G.timeserver);
    printSafeString("Zeitzone  : ", G.timezone);
    printSafeString("Lauftext  : ", G.scrollingText);
    logger.printf("H6        : %u\n", G.h6);
    logger.printf("H8        : %u\n", G.h8);
    logger.printf("H12       : %u\n", G.h12);
    logger.printf("H16       : %u\n", G.h16);
    logger.printf("H18       : %u\n", G.h18);
    logger.printf("H20       : %u\n", G.h20);
    logger.printf("H22       : %u\n", G.h22);
    logger.printf("H24       : %u\n", G.h24);
    logger.printf("ClockType    : %u\n", G.clockTypeDef);

    logger.printf("MQTT_State    : %u\n", G.mqtt.state);
    printSafeString("MQTT_Server    : ", G.mqtt.serverAdress);
    printSafeString("MQTT_User    : ", G.mqtt.user);
    printMaskedPassword();
    printSafeString("MQTT_ClientId    : ", G.mqtt.clientId);
    printSafeString("MQTT_Topic    : ", G.mqtt.topic);
    logger.printf("MQTT_Port    : %u\n", G.mqtt.port);

    logger.printf("autoBrightEnabled    : %u\n", G.autoBrightEnabled);
    logger.printf("autoBrightMin    : %u\n", G.autoBrightMin);
    logger.printf("autoBrightMax    : %u\n", G.autoBrightMax);
    logger.printf("autoBrightPeak    : %u\n", G.autoBrightPeak);
    logger.printf("transitionDuration    : %u\n", G.transitionDuration);
    logger.printf("transitionType    : %u\n", G.transitionType);
    logger.printf("colorize    : %u\n", G.colorize);
    logger.printf("transitionDemo    : %u\n", G.transitionDemo);

    logger.printf("bootLedBlink    : %u\n", G.bootLedBlink);
    logger.printf("bootLedSweep    : %u\n", G.bootLedSweep);
    logger.printf("bootShowWifi    : %u\n", G.bootShowWifi);
    logger.printf("bootShowIP    : %u\n", G.bootShowIP);
    logger.printf("ledPin    : %u\n", G.hardwarePins.led);
    logger.printf("powerButtonPin    : %u\n", G.hardwarePins.powerButton);
    logger.printf("modeButtonPin    : %u\n", G.hardwarePins.modeButton);
    logger.printf("speedButtonPin    : %u\n", G.hardwarePins.speedButton);
    logger.printf("i2cSdaPin    : %u\n", G.i2cSdaPin);
    logger.printf("i2cSclPin    : %u\n", G.i2cSclPin);
    logger.printf("Colortype    : %u\n", G.Colortype);
    printMaskedOpenWeatherMapApiKey();
    printSafeString("OWM_city  : ", G.openWeatherMap.cityid);

    for (uint8_t i = 0; i < MAX_BIRTHDAY_COUNT; i++) {
        logger.printf("Birthday%1u: %02u.%02u\n", i, G.birthday[i].day,
                      G.birthday[i].month);
    }
}

} // namespace detail

//------------------------------------------------------------------------------

template <class T> int writeAnything(int ee, const T &value) {
    const byte *p = (const byte *)(const void *)&value;
    uint32_t i;
    for (i = 0; i < sizeof(value); i++)
        EEPROM.write(ee++, *p++);
    return i;
}

//------------------------------------------------------------------------------

template <class T> int readAnything(int ee, T &value) {
    byte *p = (byte *)(void *)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
        *p++ = EEPROM.read(ee++);
    return i;
}

//------------------------------------------------------------------------------

void write() {
    writeAnything(0, G);
    EEPROM.commit();
}

//------------------------------------------------------------------------------

void read() {
    readAnything(0, G);

#if GENERAL_VERBOSE
    detail::printConfig();
#endif

    delay(100);
}
} // namespace eeprom
