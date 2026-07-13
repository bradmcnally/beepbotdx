#include "platform/memory.h"

static const uint32_t TOTAL_BUDGET = 8 * 1024 * 1024;
static uint32_t _used = 0;

void Memory::init() {
    _used = 0;
}

uint32_t Memory::getSampleBudget() {
    return TOTAL_BUDGET;
}

uint32_t Memory::getFree() {
    return TOTAL_BUDGET > _used + 30000 ? TOTAL_BUDGET - _used - 30000 : 0;
}

void Memory::trackAlloc(uint32_t bytes) {
    _used += bytes;
}

void Memory::trackFree(uint32_t bytes) {
    if (bytes > _used) _used = 0;
    else _used -= bytes;
}
