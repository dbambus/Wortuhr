#include "TelnetLog.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

TelnetLog logger;

namespace {

constexpr uint16_t TELNET_PORT = 23;
constexpr size_t HISTORY_SIZE = 2048;

WiFiServer server(TELNET_PORT);
WiFiClient client;
bool serverStarted = false;

uint8_t history[HISTORY_SIZE];
size_t historyStart = 0;
size_t historyLength = 0;

} // namespace

//------------------------------------------------------------------------------

void TelnetLog::begin() {
    server.begin();
    server.setNoDelay(true);
    serverStarted = true;
}

//------------------------------------------------------------------------------

void TelnetLog::loop() {
    if (!serverStarted) {
        return;
    }

#ifdef ESP8266
    WiFiClient incoming = server.accept();
#else
    WiFiClient incoming = server.available();
#endif
    if (incoming) {
        client.stop();
        client = incoming;
        client.setNoDelay(true);
#ifdef ESP8266
        client.setTimeout(200);
#endif
        replay();
    }

    if (client && !client.connected()) {
        client.stop();
    }
    while (client && client.available()) {
        client.read();
    }
}

//------------------------------------------------------------------------------

size_t TelnetLog::write(uint8_t c) { return write(&c, 1); }

//------------------------------------------------------------------------------

size_t TelnetLog::write(const uint8_t *buffer, size_t size) {
    Serial.write(buffer, size);
    remember(buffer, size);
    if (client && client.connected()) {
        client.write(buffer, size);
    }
    return size;
}

//------------------------------------------------------------------------------

void TelnetLog::remember(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        history[(historyStart + historyLength) % HISTORY_SIZE] = buffer[i];
        if (historyLength < HISTORY_SIZE) {
            historyLength++;
        } else {
            historyStart = (historyStart + 1) % HISTORY_SIZE;
        }
    }
}

//------------------------------------------------------------------------------

void TelnetLog::replay() {
    const size_t firstPart = min(historyLength, HISTORY_SIZE - historyStart);
    client.write(history + historyStart, firstPart);
    client.write(history, historyLength - firstPart);
}
