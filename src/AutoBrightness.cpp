#include "AutoBrightness.h"

#include "Config.h"
#include "I2CBus.h"
#include "WordClockState.h"
#include <Arduino.h>

void AutoBrightness::begin() {
#if defined(ESP32)
    analogReadResolution(10);
#endif

    // Find BH1750 and initialize if available else fallback to LDR if available
    byte bh1750Address = i2cBus::findBH1750Address(G.i2cSdaPin, G.i2cSclPin);

    if (bh1750Address != 0) {
        m_lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, bh1750Address);
        Serial.println("BH1750 initialized");
        m_usesBH1750 = true;
    } else {
        Serial.println("BH1750 initialisation error, using LDR if available");
        m_usesBH1750 = false;
    }
}

//------------------------------------------------------------------------------

/*
Automatic Brightness Control (enabled/disabled by G.autoBrightEnabled)

1. Measure ambient light with sensor BH1750, if available. If not, use LDR.
2. Derive the ledGain for the LEDs 0.0 - 100.0%
3. When ledGain changed, set parametersChanged = true

Inputs:
G.autoBrightEnabled, G.autoBrightMin, G.autoBrightMax, G.autoBrightPeak
AUTOBRIGHT_LDR_RESBRIGHT, AUTOBRIGHT_LDR_RESDARK, AUTOBRIGHT_LDR_RESDIVIDER

Outputs:
lux = Ambient light [LUX]
ledGain = gain for LEDs: 0.0-100.0% (n % of the configured bri:effectBri)
*/

void AutoBrightness::loop() {
    if (G.autoBrightEnabled != 1)
        return;

    if (G.autoBrightMin == G.autoBrightMax) {
        // If min and max are identical, nothing needs to be measured.
        ledGain = G.autoBrightMax;
        return;
    }

    float luxNow;
    if (!measureLux(luxNow)) {
        return;
    }

    /*
    Even in HIGH_RES_MODE, the BH1750 only has a precision of 1 lux. The decimal
    point values supplied by the library should therefore be considered as
    'noise' and their use makes little to no sense. Also: in order to not
    trigger parametersChanged more often than necessary, the value gets rounded.
    */

    m_lux = round(luxNow);

    applyGainForLux();
}

//------------------------------------------------------------------------------

bool AutoBrightness::measureLux(float &lux) {
    if (!m_usesBH1750) {
        lux = measureLuxWithLdr();
        return true;
    }

    if (!m_lightMeter.measurementReady()) {
        return false;
    }
    lux = m_lightMeter.readLightLevel(); // 0.0-54612.5 LUX
    return lux >= 0;
}

//------------------------------------------------------------------------------

float AutoBrightness::measureLuxWithLdr() {
    /*
    The lux value is considerably misrepresented upwards at ADC values above
    980. As 980 with an LDR5528 corresponds to approx. 1500 lux, but usually
    no more than 1000 lux are reached indoors even on sunny days, a fixed
    upper measurement limit of 980 is set here. Regardless of this, a
    maximum value of 1023 has to be applied in any case in order to avoid
    division by zero (analogRead is 0-1023)!
    */
    uint16_t adcValue = analogRead(A0);
    if (adcValue > 980) {
        adcValue = 980;
    }

    return (adcValue * AUTOBRIGHT_LDR_RESDARK * 10.0f) /
           (AUTOBRIGHT_LDR_RESBRIGHT * AUTOBRIGHT_LDR_RESDIVIDER *
            (1024.0f - adcValue));
}

//------------------------------------------------------------------------------

void AutoBrightness::applyGainForLux() {
    const float ledGainOld = ledGain;

    ledGain = map(m_lux, 0, G.autoBrightPeak, G.autoBrightMin, G.autoBrightMax);

    // Ensure ledGain meets the defined limits.
    uint16_t minimum = min(G.autoBrightMin, G.autoBrightMax);
    uint16_t maximum = max(G.autoBrightMin, G.autoBrightMax);

    if (ledGain >= maximum) {
        ledGain = maximum;
    }
    if (ledGain <= minimum) {
        ledGain = minimum;
    }

    if (ledGainOld != ledGain) {
        parametersChanged = true;
    }
}
