#pragma once

#include <cstdint>
#include "slot_fx.h"

struct SoundSlot {
    int16_t* samples;
    uint32_t length;
    uint32_t allocLength;
    uint32_t sampleRate;
    char name[9];
    uint8_t level; // 0-100 percent
    bool occupied;
    SlotFx fx;
};

namespace SoundSlotOps {

void init(SoundSlot& slot);
bool allocate(SoundSlot& slot, uint32_t maxLength);
bool shrinkToFit(SoundSlot& slot);
void free(SoundSlot& slot);
void setName(SoundSlot& slot, const char* name);

}
