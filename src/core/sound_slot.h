#pragma once

#include <cstdint>
#include "slot_fx.h"

enum BitDepth : uint8_t {
    BIT_DEPTH_16 = 0,
    BIT_DEPTH_8 = 1,
};

struct SoundSlot {
    int16_t* samples;
    uint32_t length;
    uint32_t allocLength;
    uint32_t sampleRate;
    BitDepth bitDepth;
    char name[9];
    uint8_t level; // 0-100 percent
    bool occupied;
    bool normalize = false;
    float normalizationGain = 1.0f;
    SlotFx fx;
};

namespace SoundSlotOps {

// Called before an arena allocation is released or moved.
void setBeforeArenaMutation(void (*callback)());

void init(SoundSlot& slot);
bool allocate(SoundSlot& slot, uint32_t maxLength, BitDepth bitDepth = BIT_DEPTH_16);
bool shrinkToFit(SoundSlot& slot);
void free(SoundSlot& slot);
void setName(SoundSlot& slot, const char* name);
uint8_t bytesPerSample(BitDepth bitDepth);
uint32_t allocatedBytes(const SoundSlot& slot);
int16_t getSample(const SoundSlot& slot, uint32_t index);
void setSample(SoundSlot& slot, uint32_t index, int16_t sample);
// Playback-only boost; stored PCM and mix level remain unchanged.
void updateNormalizationGain(SoundSlot& slot);
int16_t getPlaybackSample(const SoundSlot& slot, uint32_t index);
uint32_t capacityBytes();
uint32_t usedBytes();
uint32_t freeBytes();
uint32_t availableSamples(BitDepth bitDepth, const SoundSlot* replacing = nullptr);

}
