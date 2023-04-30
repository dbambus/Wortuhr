#include <arduino_homekit_server.h>
#include "Uhr.h"

#define LOG_D(fmt, ...) printf_P(PSTR(fmt "\n"), ##__VA_ARGS__);

bool received_sat = false;
bool received_hue = false;

float current_brightness = 100.0;
float current_sat = 0.0;
float current_hue = 0.0;

//==============================
// HomeKit setup and loop
//==============================

// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_bright;
extern "C" homekit_characteristic_t cha_sat;
extern "C" homekit_characteristic_t cha_hue;

static uint32_t next_heap_millis = 0;

void updateColor() {
    if (received_hue && received_sat) {
        received_hue = false;
        received_sat = false;
    }
    G.color[Foreground] = HsbColor(current_hue, current_sat/100.0, current_brightness/100.0);

    G.prog = COMMAND_MODE_WORD_CLOCK;
    parametersChanged = true;
}

void set_on(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on.value.bool_value = on; // sync the value

    if (on) {
        G.state = true;
        Serial.println("On");
    } else {
        G.state = false;
        Serial.println("Off");
    }

    updateColor();
}

void set_hue(const homekit_value_t v) {
    Serial.println("set_hue");
    float hue = v.float_value;
    cha_hue.value.float_value = hue; // sync the value

    current_hue = hue;
    received_hue = true;

    updateColor();
}

void set_sat(const homekit_value_t v) {
    Serial.println("set_sat");
    float sat = v.float_value;
    cha_sat.value.float_value = sat; // sync the value

    current_sat = sat;
    received_sat = true;

    updateColor();
}

void set_bright(const homekit_value_t v) {
    Serial.println("set_bright");
    int bright = v.int_value;
    cha_bright.value.int_value = bright; // sync the value

    current_brightness = bright;

    updateColor();
}

void my_homekit_setup() {

    cha_on.setter = set_on;
    cha_bright.setter = set_bright;
    cha_sat.setter = set_sat;
    cha_hue.setter = set_hue;

    arduino_homekit_setup(&accessory_config);
}

void my_homekit_loop() {
    arduino_homekit_loop();
    const uint32_t t = millis();
    if (t > next_heap_millis) {
        // show heap info every 5 seconds
        next_heap_millis = t + 5 * 1000;
        LOG_D("Free heap: %d, HomeKit clients: %d", ESP.getFreeHeap(),
              arduino_homekit_connected_clients_count());
    }
}