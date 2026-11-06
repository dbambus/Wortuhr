#pragma once

#include "ClockWork.h"
#include "WordClockState.h"
#include <ArduinoJson.h>

class Mqtt {
private:
    ClockWork &clockWork;
    void reInit();
    void publishDiscoveryConfigs();
    static void callback(char *receivedTopic, byte *payload,
                         unsigned int length);
    static bool processState(const JsonDocument &doc);
    static bool processEffect(const JsonDocument &doc);
    static void processScrollingText(const JsonDocument &doc);
    static bool processColor(const JsonDocument &doc);
    static bool processBrightness(const JsonDocument &doc);
    static bool checkIfMqttUserIsEmpty();
    static const char *getEffectName();

    static void handleHomeAssistantStatus(const char *msg);
    static void handleAutoBrightnessCommand(const char *msg);
    static bool handleTransitionCommand(const String &topicStr,
                                        const char *msg);
    static void handleJsonCommand(const String &topicStr, const char *msg);
    static void handleLightCommand(const JsonDocument &doc);
    static void handleEffectSpeedCommand(const char *msg);

    static void publishLightState();
    void publishDiagnostics();
    static void publishEffectSpeedState();
    static void publishScrollingTextState();
    static void publishAutoBrightnessState();
    static void publishTransitionStates();

public:
    Mqtt(ClockWork &cw);
    ~Mqtt();

    void init();
    void loop();
    void sendState();
    void sendDiscovery();

    bool isConnected();
    const char *status();
    void notifyStatus();
};

extern Mqtt *mqttInstance;
