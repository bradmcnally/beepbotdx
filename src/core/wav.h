#pragma once
#include <cstddef>
#include <cstdint>
#include "sound_slot.h"

namespace Wav {
class Stream {
public:
    virtual ~Stream() = default;
    virtual size_t read(void* data, size_t size) = 0;
    virtual size_t write(const void* data, size_t size) = 0;
    virtual uint32_t tell() = 0;
    virtual bool seek(uint32_t position) = 0;
};
bool load(Stream& file, SoundSlot& slot, const char* path, BitDepth targetBitDepth);
bool save(Stream& file, const SoundSlot& slot);
}
