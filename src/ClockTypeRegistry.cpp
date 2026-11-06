#include "ClockTypeRegistry.h"

#include "WordClockState.h"

#include "ClockType.gen.h"

ClockType *clockTypeFor(uint8_t type) {
    switch (type) {
#define X(name, id, var, i18n)                                                 \
    case name:                                                                 \
        return &var;
        CLOCK_TYPES_LIST
#undef X
    default:
        return nullptr;
    }
}
