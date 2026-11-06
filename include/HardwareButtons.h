#pragma once

#include "HardwareButtonController.h"

class HardwareButtons {
public:
    void begin(const HardwarePins &hardwarePins);
    void loop();

private:
    void handleAction(HardwareButtonAction action);
    void togglePower();
    void nextMode();
    void nextTransition();
    void increaseBrightness();
    void nextHue();
    void requestDisplayRefresh();

    HardwareButtonController m_controller;
};
