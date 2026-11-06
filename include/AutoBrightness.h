#pragma once

#include <BH1750.h>
#include <stdint.h>

class AutoBrightness {
public:
    void begin();
    void loop();

    uint32_t lux() const { return m_lux; }
    bool usesBH1750() const { return m_usesBH1750; }

private:
    bool measureLux(float &lux);
    float measureLuxWithLdr();
    void applyGainForLux();

    BH1750 m_lightMeter;
    uint32_t m_lux = 0;
    bool m_usesBH1750 = false;
};
