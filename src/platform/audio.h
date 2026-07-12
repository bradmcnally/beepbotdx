#pragma once

#include <cstdint>

struct SlotFx;

namespace Audio {

void init();

void recordStart(int16_t* buffer, uint32_t maxLength);
void recordUpdate();
void recordStop();
bool isRecording();
uint32_t getRecordedLength();

void triggerSound(const int16_t* buffer, uint32_t length, uint32_t sampleRate, uint8_t volume = 255, const SlotFx* fx = nullptr);
void stopAll();

void setVolume(uint8_t vol);
uint8_t getVolume();

}
