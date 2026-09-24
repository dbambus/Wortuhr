/*
 * MQTT and the brightness button dim the whole face through the primary
 * colour. In polychrome mode the gradient end has to follow, or the ramp tilts
 * into light-to-dark instead of getting darker as a whole.
 */

#include "Render/DisplayBrightness.h"
#include "support/check.h"
#include <math.h>

namespace {

char message[160];

bool near(float a, float b) { return fabsf(a - b) < 1e-5f; }

void configure(HsbColor (&colors)[ColorPositionCount], float primary,
               float gradientEnd) {
    colors[Foreground] = HsbColor(0.1f, 0.8f, primary);
    colors[Background] = HsbColor(0.3f, 0.5f, 0.2f);
    colors[Frame] = HsbColor(0.5f, 0.6f, 0.7f);
    colors[GradientEnd] = HsbColor(0.7f, 0.9f, gradientEnd);
}

void bothEndsDimTogether() {
    HsbColor colors[ColorPositionCount];
    configure(colors, 1.f, 1.f);

    setDisplayBrightness(colors, 0.2f);

    check(near(colors[Foreground].B, 0.2f), "the primary takes the brightness");
    check(near(colors[GradientEnd].B, 0.2f),
          "an even ramp stays even when dimmed");
}

void theConfiguredRatioIsKept() {
    HsbColor colors[ColorPositionCount];
    configure(colors, 0.8f, 0.4f);

    setDisplayBrightness(colors, 0.4f);
    snprintf(message, sizeof message,
             "the gradient end keeps half the primary (got %.3f)",
             colors[GradientEnd].B);
    check(near(colors[GradientEnd].B, 0.2f), message);

    setDisplayBrightness(colors, 0.8f);
    check(near(colors[GradientEnd].B, 0.4f),
          "brightening back restores the configured gradient end");
}

void onlyTheBrightnessChanges() {
    HsbColor colors[ColorPositionCount];
    configure(colors, 0.8f, 0.4f);

    setDisplayBrightness(colors, 0.5f);

    check(near(colors[Foreground].H, 0.1f) && near(colors[Foreground].S, 0.8f),
          "the primary keeps its hue and saturation");
    check(near(colors[GradientEnd].H, 0.7f) &&
              near(colors[GradientEnd].S, 0.9f),
          "the gradient end keeps its hue and saturation");
    check(near(colors[Background].B, 0.2f) && near(colors[Frame].B, 0.7f),
          "background and frame are not part of the face brightness");
}

void aBrighterGradientEndIsCapped() {
    HsbColor colors[ColorPositionCount];
    configure(colors, 0.5f, 1.f);

    setDisplayBrightness(colors, 0.8f);

    check(near(colors[GradientEnd].B, 1.f),
          "the gradient end never goes past full brightness");
}

void aDarkPrimaryHandsOverItsBrightness() {
    HsbColor colors[ColorPositionCount];
    configure(colors, 0.f, 0.6f);

    setDisplayBrightness(colors, 0.3f);

    check(near(colors[Foreground].B, 0.3f), "the primary comes back up");
    check(near(colors[GradientEnd].B, 0.3f),
          "without a ratio to keep, both ends take the new brightness");
}

} // namespace

int main() {
    bothEndsDimTogether();
    theConfiguredRatioIsKept();
    onlyTheBrightnessChanges();
    aBrighterGradientEndIsCapped();
    aDarkPrimaryHandsOverItsBrightness();

    return report("display brightness");
}
