#include "sound_slot.h"
#include "config.h"
#include <cstring>

#if !defined(NATIVE_TEST) && !defined(DESKTOP_BUILD)
#include <esp_heap_caps.h>
#else
#include <cstdlib>
#endif

void SoundSlotOps::init(SoundSlot& slot) {
    slot.samples = nullptr;
    slot.length = 0;
    slot.sampleRate = SAMPLE_RATE;
    slot.name[0] = '\0';
    slot.level = 100;
    slot.occupied = false;
}

bool SoundSlotOps::allocate(SoundSlot& slot, uint32_t maxLength) {
    free(slot);

    uint32_t bytes = maxLength * sizeof(int16_t);

#if !defined(NATIVE_TEST) && !defined(DESKTOP_BUILD)
    slot.samples = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
    slot.samples = (int16_t*)malloc(bytes);
#endif

    if (!slot.samples) return false;

    memset(slot.samples, 0, bytes);
    slot.length = 0;
    slot.sampleRate = SAMPLE_RATE;
    slot.occupied = false;
    return true;
}

bool SoundSlotOps::shrinkToFit(SoundSlot& slot) {
    if (!slot.samples || slot.length == 0) return false;

    uint32_t bytes = slot.length * sizeof(int16_t);

#if !defined(NATIVE_TEST) && !defined(DESKTOP_BUILD)
    int16_t* newBuf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
    int16_t* newBuf = (int16_t*)malloc(bytes);
#endif

    if (!newBuf) return false;

    memcpy(newBuf, slot.samples, bytes);

#if !defined(NATIVE_TEST) && !defined(DESKTOP_BUILD)
    heap_caps_free(slot.samples);
#else
    ::free(slot.samples);
#endif

    slot.samples = newBuf;
    return true;
}

void SoundSlotOps::free(SoundSlot& slot) {
    if (slot.samples) {
#if !defined(NATIVE_TEST) && !defined(DESKTOP_BUILD)
        heap_caps_free(slot.samples);
#else
        ::free(slot.samples);
#endif
        slot.samples = nullptr;
    }
    slot.length = 0;
    slot.occupied = false;
}

void SoundSlotOps::setName(SoundSlot& slot, const char* name) {
    strncpy(slot.name, name, 8);
    slot.name[8] = '\0';
}
