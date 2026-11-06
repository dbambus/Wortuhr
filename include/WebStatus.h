#pragma once

#include "AutoBrightness.h"
#include <stdint.h>

namespace webStatus {
void sendMqttValues(uint8_t clientNr);
void sendConfigValues(uint8_t clientNr);
void sendBirthdays(uint8_t clientNr);
void sendColorValues(uint8_t clientNr);
void sendAutoBrightValues(uint8_t clientNr,
                          const AutoBrightness &autoBrightness);
void sendTransitionValues(uint8_t clientNr);
void sendI2CScan(uint8_t clientNr);
void broadcastChanges();
} // namespace webStatus
