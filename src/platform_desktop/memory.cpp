#include "platform/memory.h"

static uint32_t _sampleBudget = 512000;

void Memory::init() {
    // Desktop has plenty of memory
}

uint32_t Memory::getSampleBudget() {
    return _sampleBudget;
}

uint32_t Memory::getFree() {
    return _sampleBudget;
}
