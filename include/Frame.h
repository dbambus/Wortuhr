#pragma once

#include <stdint.h>

class SecondsFrame {
private:
    uint8_t numFramePixels;
    uint16_t frameIntervall;
    uint8_t lastSecondFrame;
    uint8_t lastSyncedSecond;
    uint16_t countMillisFrameIntervall;
    uint16_t previousMillis;

private:
    void setInitFrameSector();
    void frameLogic();
    bool checkIfFrameLoopShouldSkip() const;
    void updateMillisCounters();
    bool checkIfFrameInit() const;
    bool checkIftoRunFrameLogic() const;
    uint16_t calcCurrentSecondFrameVariable();
    void resyncToDisplayedTime();
    void handleFrameSectorToggle();
    void handleFrameSector();
    void toggleFrameSectorUpToCurrent();
    bool isFullMinute() const;
    void handleFullMinute();
    void handleSecondFrameChange();
    void updateLedsIfClockworkMode();

public:
    SecondsFrame(const uint8_t num);
    ~SecondsFrame();

    void initFrame();
    void loop();
};
