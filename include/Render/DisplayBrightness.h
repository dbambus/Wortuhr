#pragma once

#include "WordClockState.h" // ColorPosition

/*
 * MQTT and the brightness button set one brightness for the whole face, and the
 * primary colour carries it. The gradient end has a brightness of its own,
 * though: left alone it would stay bright while the primary dims, and the
 * polychrome ramp would tilt into a light-to-dark ramp instead of dimming as a
 * whole.
 *
 * So the gradient end is scaled by the same factor, keeping the ratio the two
 * colours were configured with. From a dark primary there is no ratio to keep;
 * both ends then take the new brightness.
 */
inline void setDisplayBrightness(HsbColor (&colors)[ColorPositionCount],
                                 float brightness) {
    const float previous = colors[Foreground].B;
    colors[Foreground].B = brightness;

    float gradientEnd = brightness;
    if (previous > 0.f) {
        gradientEnd = colors[GradientEnd].B * brightness / previous;
    }
    if (gradientEnd > 1.f) {
        gradientEnd = 1.f;
    } else if (gradientEnd < 0.f) {
        gradientEnd = 0.f;
    }
    colors[GradientEnd].B = gradientEnd;
}
