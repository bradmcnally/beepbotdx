#include <M5Cardputer.h>
#include "memory.h"

static uint32_t _sampleBudget = 0;

void Memory::init() {
    uint32_t freeHeap = ESP.getFreeHeap();
    // Reserve 30KB for system/stack, rest is available for samples
    _sampleBudget = freeHeap > 30000 ? freeHeap - 30000 : 0;
}

uint32_t Memory::getSampleBudget() {
    return _sampleBudget;
}
