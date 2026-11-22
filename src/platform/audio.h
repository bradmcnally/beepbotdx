#pragma once

#include <cstdint>
#include "core/sound_slot.h"

struct SlotFx;

namespace Audio {

void init();
void update();

void recordStart(void* buffer, uint32_t maxLength, BitDepth bitDepth, uint8_t gain = 1);
void recordUpdate();
void recordStop();
bool isRecording();
uint32_t getRecordedLength();

void triggerSound(const SoundSlot& slot, uint8_t volume = 255, const SlotFx* fx = nullptr);
void stopAll();

#ifdef CARDPUTER_ZERO_DEVICE
// Adjust the Zero's shared PipeWire output, returning its new percentage.
// Returns -1 if the system volume cannot be read or changed.
int adjustSystemVolume(bool increase);
#else
void setVolume(uint8_t vol);
uint8_t getVolume();
#endif

}
