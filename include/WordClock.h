#pragma once

#include <Arduino.h>

void applyTimezone();
void sendMQTTUpdate();
void setDefaultHardwarePins();
bool hardwarePinsAreValid();
void deleteActiveLedStrip();
