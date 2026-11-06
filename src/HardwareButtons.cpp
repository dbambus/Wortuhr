#include "HardwareButtons.h"

#include "Led.h"
#include "Transitions/TransitionType.h"
#include "WordClockState.h"

extern Led led;

void HardwareButtons::begin(const HardwarePins &hardwarePins) {
    m_controller.begin(hardwarePins);
}

//------------------------------------------------------------------------------

void HardwareButtons::loop() {
    const HardwareButtonAction action = m_controller.loop();
    if (action != HardwareButtonAction::None) {
        handleAction(action);
    }
}

//------------------------------------------------------------------------------

void HardwareButtons::handleAction(HardwareButtonAction action) {
    switch (action) {
    case HardwareButtonAction::TogglePower:
        togglePower();
        break;
    case HardwareButtonAction::NextMode:
        nextMode();
        break;
    case HardwareButtonAction::NextTransition:
        nextTransition();
        break;
    case HardwareButtonAction::IncreaseBrightness:
        increaseBrightness();
        break;
    case HardwareButtonAction::NextHue:
        nextHue();
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------

void HardwareButtons::togglePower() {
    led.setState(!led.getState());

    requestDisplayRefresh();
}

//------------------------------------------------------------------------------

void HardwareButtons::nextMode() {
    static const uint8_t modes[] = {
        COMMAND_MODE_WORD_CLOCK,    COMMAND_MODE_SECONDS,
        COMMAND_MODE_SCROLLINGTEXT, COMMAND_MODE_RAINBOWCYCLE,
        COMMAND_MODE_RAINBOW,       COMMAND_MODE_FIRE,
        COMMAND_MODE_COLOR,         COMMAND_MODE_DIGITAL_CLOCK,
        COMMAND_MODE_SYMBOL};

    uint8_t nextMode = modes[0];
    for (uint8_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        if (G.prog == modes[i] ||
            (G.prog == COMMAND_IDLE && modes[i] == COMMAND_MODE_WORD_CLOCK)) {
            nextMode = modes[(i + 1) % (sizeof(modes) / sizeof(modes[0]))];
            break;
        }
    }

    G.prog = nextMode;
    G.progInit = true;
    parametersChanged = true;
    webStateChanged = true;
}

//------------------------------------------------------------------------------

void HardwareButtons::nextTransition() {
    static const uint8_t transitions[] = {
        NO_TRANSITION, ROLL_UP,     ROLL_DOWN, SHIFT_LEFT, SHIFT_RIGHT, FADE,
        LASER,         MATRIX_RAIN, BALLS,     FIRE,       SNAKE,       RANDOM};

    uint8_t nextTransition = transitions[0];
    for (uint8_t i = 0; i < sizeof(transitions) / sizeof(transitions[0]); i++) {
        if (G.transitionType == transitions[i]) {
            nextTransition = transitions[(i + 1) % (sizeof(transitions) /
                                                    sizeof(transitions[0]))];
            break;
        }
    }

    G.transitionType = nextTransition;
    G.prog = COMMAND_MODE_TRANSITION;
    G.progInit = true;
    parametersChanged = true;
    webStateChanged = true;
}

//------------------------------------------------------------------------------

void HardwareButtons::increaseBrightness() {
    float brightness = G.color[Foreground].B;
    if (brightness <= 0.0f) {
        brightness = 0.1f;
    } else if (brightness >= 1.0f) {
        brightness = 0.1f;
    } else {
        brightness += 0.1f;
        if (brightness > 1.0f) {
            brightness = 1.0f;
        }
    }

    G.color[Foreground].B = brightness;
    requestDisplayRefresh();
}

//------------------------------------------------------------------------------

void HardwareButtons::nextHue() {
    float hue = G.color[Foreground].H + (30.0f / 360.0f);
    if (hue >= 1.0f) {
        hue -= 1.0f;
    }
    G.color[Foreground].H = hue;
    requestDisplayRefresh();
}

//------------------------------------------------------------------------------

void HardwareButtons::requestDisplayRefresh() {
    parametersChanged = true;
    webStateChanged = true;
    if (G.prog == COMMAND_IDLE) {
        G.prog = COMMAND_MODE_WORD_CLOCK;
    }
}
