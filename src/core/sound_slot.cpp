#include "sound_slot.h"
#include "config.h"
#include <cstring>

namespace {

constexpr uint8_t MAX_ARENA_ALLOCATIONS = NUM_SOUNDS + 2;
alignas(int16_t) uint8_t arena[SAMPLE_ARENA_BYTES];

struct ArenaAllocation {
    SoundSlot* owner;
    uint32_t offset;
    uint32_t size;
};

ArenaAllocation allocations[MAX_ARENA_ALLOCATIONS] = {};
uint8_t allocationCount = 0;
uint32_t arenaUsed = 0;
void (*beforeArenaMutation)() = nullptr;

uint32_t alignedBytes(uint32_t bytes) {
    return (bytes + alignof(int16_t) - 1) & ~(alignof(int16_t) - 1);
}

int findAllocation(const SoundSlot& slot) {
    for (uint8_t i = 0; i < allocationCount; i++) {
        if (allocations[i].owner == &slot) return i;
    }
    return -1;
}

void updatePointers(uint8_t first) {
    for (uint8_t i = first; i < allocationCount; i++) {
        allocations[i].owner->samples =
            reinterpret_cast<int16_t*>(arena + allocations[i].offset);
    }
}

void releaseAllocation(SoundSlot& slot) {
    int index = findAllocation(slot);
    if (index < 0) return;

    const uint32_t offset = allocations[index].offset;
    const uint32_t size = allocations[index].size;
    const uint32_t tailOffset = offset + size;
    const uint32_t tailSize = arenaUsed - tailOffset;
    if (tailSize > 0) {
        memmove(arena + offset, arena + tailOffset, tailSize);
    }

    for (uint8_t i = index + 1; i < allocationCount; i++) {
        allocations[i - 1] = allocations[i];
        allocations[i - 1].offset -= size;
    }
    allocationCount--;
    arenaUsed -= size;
    updatePointers(index);
}

}

#ifndef NATIVE_TEST
#include "platform/memory.h"
#else
namespace Memory { void trackAlloc(uint32_t) {} void trackFree(uint32_t) {} }
#endif

void SoundSlotOps::setBeforeArenaMutation(void (*callback)()) {
    beforeArenaMutation = callback;
}

void SoundSlotOps::init(SoundSlot& slot) {
    slot.samples = nullptr;
    slot.length = 0;
    slot.allocLength = 0;
    slot.sampleRate = SAMPLE_RATE;
    slot.bitDepth = BIT_DEPTH_16;
    slot.name[0] = '\0';
    slot.level = 100;
    slot.occupied = false;
    slot.normalize = false;
    slot.normalizationGain = 1.0f;
    SlotFxOps::defaults(slot.fx);
}

bool SoundSlotOps::allocate(SoundSlot& slot, uint32_t maxLength, BitDepth bitDepth) {
    uint32_t bytes = alignedBytes(maxLength * bytesPerSample(bitDepth));
    int existing = findAllocation(slot);
    uint32_t reclaimable = existing >= 0 ? allocations[existing].size : 0;
    if (bytes == 0 || bytes > SAMPLE_ARENA_BYTES - arenaUsed + reclaimable) {
        return false;
    }
    if (existing < 0 && allocationCount >= MAX_ARENA_ALLOCATIONS) return false;

    // Capacity is known to fit, so replacing a slot cannot destroy its old
    // sample because of an allocation failure.
    free(slot);

    ArenaAllocation& allocation = allocations[allocationCount++];
    allocation.owner = &slot;
    allocation.offset = arenaUsed;
    allocation.size = bytes;
    slot.samples = reinterpret_cast<int16_t*>(arena + arenaUsed);
    arenaUsed += bytes;

    Memory::trackAlloc(bytes);
    memset(slot.samples, 0, bytes);
    slot.length = 0;
    slot.allocLength = maxLength;
    slot.sampleRate = SAMPLE_RATE;
    slot.bitDepth = bitDepth;
    slot.level = 100;
    slot.occupied = false;
    SlotFxOps::defaults(slot.fx);
    return true;
}

bool SoundSlotOps::shrinkToFit(SoundSlot& slot) {
    if (!slot.samples || slot.length == 0) return false;

    uint32_t bytes = alignedBytes(
        slot.length * bytesPerSample(slot.bitDepth));
    int index = findAllocation(slot);
    if (index < 0 || bytes > allocations[index].size) return false;

    uint32_t released = allocations[index].size - bytes;
    if (released == 0) return true;

    if (beforeArenaMutation) beforeArenaMutation();
    uint32_t tailOffset = allocations[index].offset + allocations[index].size;
    uint32_t tailSize = arenaUsed - tailOffset;
    if (tailSize > 0) {
        memmove(arena + allocations[index].offset + bytes,
                arena + tailOffset, tailSize);
    }
    allocations[index].size = bytes;
    for (uint8_t i = index + 1; i < allocationCount; i++) {
        allocations[i].offset -= released;
    }
    arenaUsed -= released;
    updatePointers(index);
    Memory::trackFree(released);
    slot.allocLength = slot.length;
    return true;
}

void SoundSlotOps::free(SoundSlot& slot) {
    if (findAllocation(slot) >= 0) {
        if (beforeArenaMutation) beforeArenaMutation();
        Memory::trackFree(allocatedBytes(slot));
    }
    releaseAllocation(slot);
    slot.samples = nullptr;
    slot.length = 0;
    slot.allocLength = 0;
    slot.occupied = false;
    slot.normalize = false;
    slot.normalizationGain = 1.0f;
}

void SoundSlotOps::setName(SoundSlot& slot, const char* name) {
    strncpy(slot.name, name, 8);
    slot.name[8] = '\0';
}

uint8_t SoundSlotOps::bytesPerSample(BitDepth bitDepth) {
    return bitDepth == BIT_DEPTH_8 ? 1 : 2;
}

uint32_t SoundSlotOps::allocatedBytes(const SoundSlot& slot) {
    return slot.allocLength * bytesPerSample(slot.bitDepth);
}

int16_t SoundSlotOps::getSample(const SoundSlot& slot, uint32_t index) {
    if (slot.bitDepth == BIT_DEPTH_8) {
        return (int16_t)((const int8_t*)slot.samples)[index] * 256;
    }
    return slot.samples[index];
}

void SoundSlotOps::setSample(SoundSlot& slot, uint32_t index, int16_t sample) {
    if (slot.bitDepth == BIT_DEPTH_8) {
        ((int8_t*)slot.samples)[index] = (int8_t)(sample >> 8);
    } else {
        slot.samples[index] = sample;
    }
}

void SoundSlotOps::updateNormalizationGain(SoundSlot& slot) {
    slot.normalizationGain = 1.0f;
    if (!slot.samples || !slot.normalize) return;
    int32_t positive = 0, negative = 0;
    for (uint32_t i = 0; i < slot.length; ++i) {
        const int32_t sample = getSample(slot, i);
        if (sample > positive) positive = sample;
        if (-sample > negative) negative = -sample;
    }
    if (positive == 0 && negative == 0) return;
    float gain = positive ? 32767.0f / positive : 32768.0f / negative;
    if (negative && 32768.0f / negative < gain) gain = 32768.0f / negative;
    slot.normalizationGain = gain;
}

int16_t SoundSlotOps::getPlaybackSample(const SoundSlot& slot, uint32_t index) {
    const int16_t sample = getSample(slot, index);
    if (!slot.normalize) return sample;
    const float amplified = sample * slot.normalizationGain;
    if (amplified >= 32767.0f) return 32767;
    if (amplified <= -32768.0f) return -32768;
    return static_cast<int16_t>(amplified + (amplified >= 0 ? 0.5f : -0.5f));
}

uint32_t SoundSlotOps::capacityBytes() {
    return SAMPLE_ARENA_BYTES;
}

uint32_t SoundSlotOps::usedBytes() {
    return arenaUsed;
}

uint32_t SoundSlotOps::freeBytes() {
    return SAMPLE_ARENA_BYTES - arenaUsed;
}

uint32_t SoundSlotOps::availableSamples(BitDepth bitDepth, const SoundSlot* replacing) {
    uint32_t bytes = freeBytes();
    if (replacing) {
        int index = findAllocation(*replacing);
        if (index >= 0) bytes += allocations[index].size;
    }
    return bytes / bytesPerSample(bitDepth);
}
