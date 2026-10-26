#pragma once

#include <Arduino.h>

class TelnetLog : public Print {
public:
    void begin();
    void loop();

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

private:
    void remember(const uint8_t *buffer, size_t size);
    void replay();
};

extern TelnetLog logger;
