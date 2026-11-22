#include "wav.h"
#include "config.h"
#include <cmath>
#include <cstring>

static bool supportedWavFormat(uint16_t format, uint16_t channels,
                               uint16_t bitsPerSample, uint32_t sampleRate) {
    if ((channels != 1 && channels != 2) || sampleRate == 0) return false;
    if (format == 1) {
        return bitsPerSample == 8 || bitsPerSample == 16 ||
               bitsPerSample == 24 || bitsPerSample == 32;
    }
    return format == 3 && bitsPerSample == 32;
}

static bool readWavValue(Wav::Stream& file, uint16_t format, uint16_t bitsPerSample,
                         int32_t& sample) {
    if (format == 1 && bitsPerSample == 8) {
        uint8_t value;
        if (file.read(&value, 1) != 1) return false;
        sample = ((int32_t)value - 128) * 256;
    } else if (format == 1 && bitsPerSample == 16) {
        int16_t value;
        if (file.read(&value, 2) != 2) return false;
        sample = value;
    } else if (format == 1 && bitsPerSample == 24) {
        uint8_t bytes[3];
        if (file.read(bytes, 3) != 3) return false;
        int32_t value = (int32_t)((uint32_t)bytes[0] |
            ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16));
        if (value & 0x00800000) value |= (int32_t)0xFF000000;
        sample = value >> 8;
    } else if (format == 1 && bitsPerSample == 32) {
        int32_t value;
        if (file.read(&value, 4) != 4) return false;
        sample = value >> 16;
    } else if (format == 3 && bitsPerSample == 32) {
        float value;
        if (file.read(&value, 4) != 4) return false;
        if (!std::isfinite(value)) return false;
        if (value > 1.0f) value = 1.0f;
        if (value < -1.0f) value = -1.0f;
        sample = (int32_t)(value * 32767.0f);
    } else {
        return false;
    }
    return true;
}

static bool readWavFrame(Wav::Stream& file, uint16_t format, uint16_t bitsPerSample,
                         uint16_t channels, int16_t& output) {
    int32_t left;
    if (!readWavValue(file, format, bitsPerSample, left)) return false;
    int64_t mixed = left;
    if (channels == 2) {
        int32_t right;
        if (!readWavValue(file, format, bitsPerSample, right)) return false;
        mixed = ((int64_t)left + right) / 2;
    }
    if (mixed > 32767) mixed = 32767;
    if (mixed < -32768) mixed = -32768;
    output = (int16_t)mixed;
    return true;
}

bool Wav::load(Stream& file, SoundSlot& slot, const char* path, BitDepth targetBitDepth) {
    // Read RIFF header
    char riff[4], wave[4];
    uint32_t fileSize;
    if (file.read(riff, 4) != 4) { return false; }
    if (file.read(&fileSize, 4) != 4) return false;
    if (file.read(wave, 4) != 4) { return false; }

    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLE_RATE;
    uint16_t bitsPerSample = 16;
    uint16_t audioFormat = 1;
    bool fmtFound = false;

    while (true) {
        char chunkId[4];
        uint32_t chunkSize;
        if (file.read(chunkId, 4) != 4) break;
        if (file.read(&chunkSize, 4) != 4) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            long fmtStart = file.tell();
            if (chunkSize < 16) { return false; }
            uint32_t byteRate;
            uint16_t blockAlign;
            if (file.read(&audioFormat, 2) != 2 ||
                file.read(&numChannels, 2) != 2 ||
                file.read(&sampleRate, 4) != 4 ||
                file.read(&byteRate, 4) != 4 ||
                file.read(&blockAlign, 2) != 2 ||
                file.read(&bitsPerSample, 2) != 2) return false;
            fmtFound = true;
            if (!file.seek(fmtStart + chunkSize + (chunkSize & 1))) return false;

        } else if (memcmp(chunkId, "data", 4) == 0) {
            if (!fmtFound) { return false; }

            if (!supportedWavFormat(audioFormat, numChannels,
                                    bitsPerSample, sampleRate)) {
                return false;
            }

            uint32_t bytesPerSample = bitsPerSample / 8;
            uint32_t srcSamples = chunkSize / bytesPerSample / numChannels;

            double ratio = (sampleRate > SAMPLE_RATE) ? (double)sampleRate / SAMPLE_RATE : 1.0;
            uint32_t outSamples = (uint32_t)(srcSamples / ratio);
            uint32_t formatMaximum;
#ifdef CARDPUTER_ZERO
            formatMaximum = SoundSlotOps::availableSamples(targetBitDepth, &slot);
#else
            formatMaximum = MAX_SAMPLE_LENGTH *
                (targetBitDepth == BIT_DEPTH_8 ? 2 : 1);
#endif
            if (outSamples > formatMaximum) outSamples = formatMaximum;

            bool needsResample = (ratio > 1.0);

            if (!SoundSlotOps::allocate(slot, outSamples, targetBitDepth)) {
                return false;
            }

            if (!needsResample) {
                for (uint32_t i = 0; i < outSamples; i++) {
                    int16_t sample;
                    if (!readWavFrame(file, audioFormat, bitsPerSample,
                                      numChannels, sample)) {
                        SoundSlotOps::free(slot);
                        return false;
                    }
                    SoundSlotOps::setSample(slot, i, sample);
                }
            } else if (outSamples > 0) {
                int16_t first;
                int16_t second;
                if (!readWavFrame(file, audioFormat, bitsPerSample,
                                  numChannels, first)) {
                    SoundSlotOps::free(slot);
                    return false;
                }
                second = first;
                uint32_t baseIndex = 0;
                if (srcSamples > 1 &&
                    !readWavFrame(file, audioFormat, bitsPerSample,
                                  numChannels, second)) {
                    SoundSlotOps::free(slot);
                    return false;
                }
                for (uint32_t i = 0; i < outSamples; i++) {
                    double srcPos = i * ratio;
                    uint32_t idx = (uint32_t)srcPos;
                    while (baseIndex < idx) {
                        first = second;
                        baseIndex++;
                        if (baseIndex + 1 < srcSamples &&
                            !readWavFrame(file, audioFormat, bitsPerSample,
                                          numChannels, second)) {
                            SoundSlotOps::free(slot);
                            return false;
                        }
                    }
                    float frac = (float)(srcPos - idx);
                    SoundSlotOps::setSample(
                        slot, i, (int16_t)(first + frac * (second - first)));
                }
            }

            slot.length = outSamples;
            slot.sampleRate = SAMPLE_RATE;
            slot.occupied = true;

            const char* filename = strrchr(path, '/');
            if (filename) filename++;
            else filename = path;
            char nameOnly[9];
            strncpy(nameOnly, filename, 8);
            nameOnly[8] = '\0';
            char* dot = strrchr(nameOnly, '.');
            if (dot) *dot = '\0';
            SoundSlotOps::setName(slot, nameOnly);

            return true;
        } else {
            if (!file.seek(file.tell() + chunkSize + (chunkSize & 1))) return false;
        }
    }

    return false;
}

bool Wav::save(Stream& file, const SoundSlot& slot) {
    if (!slot.samples || slot.length == 0) return false;
    uint8_t bytesPerSample = SoundSlotOps::bytesPerSample(slot.bitDepth);
    uint32_t dataSize = slot.length * bytesPerSample;
    uint32_t fileSize = 36 + dataSize;

    bool written = true;
    auto writeExact = [&](const void* data, size_t size) {
        if (written && file.write(data, size) != size) {
            written = false;
        }
    };

    // RIFF header
    writeExact("RIFF", 4);
    writeExact(&fileSize, 4);
    writeExact("WAVE", 4);

    // fmt chunk
    writeExact("fmt ", 4);
    uint32_t fmtSize = 16;
    writeExact(&fmtSize, 4);
    uint16_t audioFormat = 1;
    writeExact(&audioFormat, 2);
    uint16_t numChannels = 1;
    writeExact(&numChannels, 2);
    writeExact(&slot.sampleRate, 4);
    uint32_t byteRate = slot.sampleRate * bytesPerSample;
    writeExact(&byteRate, 4);
    uint16_t blockAlign = bytesPerSample;
    writeExact(&blockAlign, 2);
    uint16_t bitsPerSample = bytesPerSample * 8;
    writeExact(&bitsPerSample, 2);

    // data chunk
    writeExact("data", 4);
    writeExact(&dataSize, 4);
    if (slot.bitDepth == BIT_DEPTH_8) {
        for (uint32_t i = 0; i < slot.length; i++) {
            uint8_t sample = (uint8_t)(((const int8_t*)slot.samples)[i] + 128);
            writeExact(&sample, 1);
        }
    } else {
        writeExact(slot.samples, dataSize);
    }

    return written;
}
