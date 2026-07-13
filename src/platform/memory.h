#pragma once

#include <cstdint>

namespace Memory {

void init();
uint32_t getSampleBudget();
uint32_t getFree();
void trackAlloc(uint32_t bytes);
void trackFree(uint32_t bytes);

}
