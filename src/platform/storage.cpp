#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "storage.h"
#include "core/project_header.h"
#include "core/fx_dsp.h"
#include "config.h"
#include <cstring>
#include <cstdio>
#include <cstddef>

#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#endif

static bool _sdReady = false;

bool Storage::init() {
    SPI.begin(SD_SPI_CLK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
    _sdReady = SD.begin(SD_SPI_CS, SPI);
    Serial.printf("[storage] SD init: %s\n", _sdReady ? "OK" : "FAIL");
    return _sdReady;
}

bool Storage::isReady() {
    return _sdReady;
}

bool Storage::loadWav(SoundSlot& slot, const char* path) {
    Serial.printf("[wav] opening: %s\n", path);

    File file = SD.open(path);
    if (!file) {
        Serial.println("[wav] file open failed");
        return false;
    }

    Serial.printf("[wav] file size: %d\n", file.size());

    // Read RIFF header (12 bytes)
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    if (file.read((uint8_t*)riff, 4) != 4) { file.close(); Serial.println("[wav] can't read RIFF"); return false; }
    file.read((uint8_t*)&fileSize, 4);
    if (file.read((uint8_t*)wave, 4) != 4) { file.close(); Serial.println("[wav] can't read WAVE"); return false; }

    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
        Serial.printf("[wav] not RIFF/WAVE: %.4s / %.4s\n", riff, wave);
        file.close();
        return false;
    }

    // Parse chunks to find fmt and data
    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLE_RATE;
    uint16_t bitsPerSample = 16;
    uint16_t audioFormat = 1;
    bool fmtFound = false;

    while (file.available() >= 8) {
        char chunkId[4];
        uint32_t chunkSize;
        file.read((uint8_t*)chunkId, 4);
        file.read((uint8_t*)&chunkSize, 4);

        Serial.printf("[wav] chunk: %.4s size: %u pos: %u\n", chunkId, chunkSize, (unsigned)file.position());

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            size_t fmtStart = file.position();
            file.read((uint8_t*)&audioFormat, 2);
            file.read((uint8_t*)&numChannels, 2);
            file.read((uint8_t*)&sampleRate, 4);
            uint32_t byteRate;
            file.read((uint8_t*)&byteRate, 4);
            uint16_t blockAlign;
            file.read((uint8_t*)&blockAlign, 2);
            file.read((uint8_t*)&bitsPerSample, 2);
            fmtFound = true;

            Serial.printf("[wav] fmt: format=%u ch=%u rate=%u bits=%u\n",
                          audioFormat, numChannels, sampleRate, bitsPerSample);

            // Seek past any extra fmt bytes
            file.seek(fmtStart + chunkSize);

        } else if (memcmp(chunkId, "data", 4) == 0) {
            if (!fmtFound) {
                Serial.println("[wav] data before fmt");
                file.close();
                return false;
            }

            // Accept PCM (1) or IEEE float (3)
            if (audioFormat != 1 && audioFormat != 3) {
                Serial.printf("[wav] unsupported format: %u\n", audioFormat);
                file.close();
                return false;
            }

            uint32_t bytesPerSample = bitsPerSample / 8;
            uint32_t srcSamples = chunkSize / bytesPerSample / numChannels;

            // Downsample to SAMPLE_RATE if source is higher
            double ratio = (sampleRate > SAMPLE_RATE) ? (double)sampleRate / SAMPLE_RATE : 1.0;
            uint32_t outSamples = (uint32_t)(srcSamples / ratio);
            if (outSamples > MAX_SAMPLE_LENGTH) outSamples = MAX_SAMPLE_LENGTH;

            uint32_t srcNeeded = (uint32_t)(outSamples * ratio) + 2;
            if (srcNeeded > srcSamples) srcNeeded = srcSamples;

            Serial.printf("[wav] %u src @ %uHz -> %u out @ %uHz\n", srcSamples, sampleRate, outSamples, SAMPLE_RATE);

            if (!SoundSlotOps::allocate(slot, outSamples)) {
                Serial.println("[wav] alloc failed");
                file.close();
                return false;
            }

            // Read source samples into a temp buffer for resampling
            // If no resampling needed, read directly into slot
            int16_t* readBuf = nullptr;
            bool needsResample = (ratio > 1.0);
            if (needsResample) {
                readBuf = (int16_t*)malloc(srcNeeded * sizeof(int16_t));
                if (!readBuf) {
                    Serial.println("[wav] resample buf alloc failed");
                    SoundSlotOps::free(slot);
                    file.close();
                    return false;
                }
            }

            uint32_t samplesToRead = needsResample ? srcNeeded : outSamples;
            for (uint32_t i = 0; i < samplesToRead; i++) {
                int32_t sample = 0;

                if (audioFormat == 1 && bitsPerSample == 8) {
                    uint8_t s;
                    file.read(&s, 1);
                    sample = ((int16_t)s - 128) << 8;
                    if (numChannels == 2) {
                        uint8_t r;
                        file.read(&r, 1);
                        sample = (sample + (((int16_t)r - 128) << 8)) / 2;
                    }
                } else if (audioFormat == 1 && bitsPerSample == 16) {
                    int16_t s;
                    file.read((uint8_t*)&s, 2);
                    sample = s;
                    if (numChannels == 2) {
                        int16_t r;
                        file.read((uint8_t*)&r, 2);
                        sample = (sample + r) / 2;
                    }
                } else if (audioFormat == 1 && bitsPerSample == 24) {
                    uint8_t b[3];
                    file.read(b, 3);
                    sample = (int32_t)((b[2] << 24) | (b[1] << 16) | (b[0] << 8)) >> 8;
                    sample >>= 8;
                    if (numChannels == 2) {
                        file.read(b, 3);
                        int32_t r = (int32_t)((b[2] << 24) | (b[1] << 16) | (b[0] << 8)) >> 8;
                        r >>= 8;
                        sample = (sample + r) / 2;
                    }
                } else if (audioFormat == 1 && bitsPerSample == 32) {
                    int32_t s;
                    file.read((uint8_t*)&s, 4);
                    sample = s >> 16;
                    if (numChannels == 2) {
                        int32_t r;
                        file.read((uint8_t*)&r, 4);
                        sample = (sample + (r >> 16)) / 2;
                    }
                } else if (audioFormat == 3 && bitsPerSample == 32) {
                    float f;
                    file.read((uint8_t*)&f, 4);
                    sample = (int32_t)(f * 32767.0f);
                    if (numChannels == 2) {
                        float fr;
                        file.read((uint8_t*)&fr, 4);
                        sample = (sample + (int32_t)(fr * 32767.0f)) / 2;
                    }
                } else {
                    Serial.printf("[wav] unsupported: fmt=%u bits=%u\n", audioFormat, bitsPerSample);
                    if (readBuf) free(readBuf);
                    SoundSlotOps::free(slot);
                    file.close();
                    return false;
                }

                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;

                if (needsResample) {
                    readBuf[i] = (int16_t)sample;
                } else {
                    slot.samples[i] = (int16_t)sample;
                }
            }

            if (needsResample) {
                for (uint32_t i = 0; i < outSamples; i++) {
                    double srcPos = i * ratio;
                    uint32_t idx = (uint32_t)srcPos;
                    float frac = (float)(srcPos - idx);
                    int16_t s0 = readBuf[idx];
                    int16_t s1 = (idx + 1 < srcNeeded) ? readBuf[idx + 1] : s0;
                    slot.samples[i] = (int16_t)(s0 + frac * (s1 - s0));
                }
                free(readBuf);
            }

            slot.length = outSamples;
            slot.sampleRate = SAMPLE_RATE;
            slot.occupied = true;

            // Extract filename for slot name
            const char* filename = strrchr(path, '/');
            if (filename) filename++;
            else filename = path;
            char nameOnly[9];
            strncpy(nameOnly, filename, 8);
            nameOnly[8] = '\0';
            char* dot = strrchr(nameOnly, '.');
            if (dot) *dot = '\0';
            SoundSlotOps::setName(slot, nameOnly);

            Serial.printf("[wav] loaded OK: %s, %u samples @ %u Hz\n", slot.name, slot.length, slot.sampleRate);
            file.close();
            return true;
        } else {
            // Skip unknown chunk
            file.seek(file.position() + chunkSize);
        }
    }

    Serial.println("[wav] no data chunk found");
    file.close();
    return false;
}

bool Storage::saveWav(const SoundSlot& slot, const char* path) {
    if (!slot.samples || slot.length == 0) return false;

    SD.mkdir("/beepbotdx");
    SD.mkdir("/beepbotdx/samples");

    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;

    uint32_t dataSize = slot.length * sizeof(int16_t);
    uint32_t fileSize = 36 + dataSize;

    // RIFF header
    file.write((const uint8_t*)"RIFF", 4);
    file.write((const uint8_t*)&fileSize, 4);
    file.write((const uint8_t*)"WAVE", 4);

    // fmt chunk
    file.write((const uint8_t*)"fmt ", 4);
    uint32_t fmtSize = 16;
    file.write((const uint8_t*)&fmtSize, 4);
    uint16_t audioFormat = 1;
    file.write((const uint8_t*)&audioFormat, 2);
    uint16_t numChannels = 1;
    file.write((const uint8_t*)&numChannels, 2);
    file.write((const uint8_t*)&slot.sampleRate, 4);
    uint32_t byteRate = slot.sampleRate * 2;
    file.write((const uint8_t*)&byteRate, 4);
    uint16_t blockAlign = 2;
    file.write((const uint8_t*)&blockAlign, 2);
    uint16_t bitsPerSample = 16;
    file.write((const uint8_t*)&bitsPerSample, 2);

    // data chunk
    file.write((const uint8_t*)"data", 4);
    file.write((const uint8_t*)&dataSize, 4);
    file.write((const uint8_t*)slot.samples, dataSize);

    file.close();
    Serial.printf("[wav] saved: %s (%u samples)\n", path, slot.length);
    return true;
}

bool Storage::listWavFiles(const char* dir, char names[][32], uint8_t& count, uint8_t max) {
    count = 0;
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) return false;

    File entry = root.openNextFile();
    while (entry && count < max) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            if (name[0] == '.') { entry = root.openNextFile(); continue; }
            size_t len = strlen(name);
            if (len > 4 && strcasecmp(name + len - 4, ".wav") == 0) {
                strncpy(names[count], name, 31);
                names[count][31] = '\0';
                count++;
            }
        }
        entry = root.openNextFile();
    }

    root.close();
    return true;
}


static void projectDir(uint8_t slot, char* buf, size_t len) {
    snprintf(buf, len, "/beepbotdx/%02d", slot + 1);
}

static void projectPath(uint8_t slot, char* buf, size_t len) {
    snprintf(buf, len, "/beepbotdx/%02d/project.dat", slot + 1);
}

bool Storage::projectExists(uint8_t slot) {
    if (!_sdReady || slot >= 8) return false;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return false;
    file.close();
    return true;
}

bool Storage::deleteProject(uint8_t slot) {
    if (!_sdReady || slot >= 8) return false;
    char path[48];
    projectPath(slot, path, sizeof(path));
    return SD.remove(path);
}

static bool readHeader(File& file, ProjectHeader& hdr) {
    memset(&hdr, 0, sizeof(hdr));
    size_t fileSize = file.size();
    static const size_t fxFieldsSize = sizeof(hdr.fxValues) + sizeof(hdr.fxEnabled);
    static const size_t fxFieldsOffset = offsetof(ProjectHeader, fxValues);
    if (fileSize <= sizeof(hdr) - fxFieldsSize) {
        uint8_t* p = (uint8_t*)&hdr;
        file.read(p, fxFieldsOffset);
        size_t remaining = fileSize - fxFieldsOffset;
        file.read(p + fxFieldsOffset + fxFieldsSize, remaining);
    } else {
        file.read((uint8_t*)&hdr, sizeof(hdr));
    }
    return (hdr.magic == PROJECT_MAGIC && hdr.version >= 1 && hdr.version <= PROJECT_VERSION);
}

uint8_t Storage::loadProjectTheme(uint8_t slot) {
    if (!_sdReady || slot >= 8) return 0;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return 0;
    ProjectHeader hdr;
    if (!readHeader(file, hdr)) { file.close(); return 0; }
    file.close();
    return hdr.themeIndex;
}

bool Storage::saveProjectTheme(uint8_t slot, uint8_t themeIndex) {
    if (!_sdReady || slot >= 8) return false;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return false;
    ProjectHeader hdr;
    if (!readHeader(file, hdr)) { file.close(); return false; }
    file.close();
    hdr.themeIndex = themeIndex;
    hdr.version = PROJECT_VERSION;
    file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    file.write((const uint8_t*)&hdr, sizeof(hdr));
    file.close();
    return true;
}

bool Storage::saveProjectName(uint8_t slot, const char* name) {
    if (!_sdReady || slot >= 8) return false;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return false;
    ProjectHeader hdr;
    if (!readHeader(file, hdr)) { file.close(); return false; }
    file.close();
    strncpy(hdr.name, name, 8);
    hdr.name[8] = '\0';
    hdr.version = PROJECT_VERSION;
    file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    file.write((const uint8_t*)&hdr, sizeof(hdr));
    file.close();
    return true;
}

uint16_t Storage::loadProjectBpm(uint8_t slot) {
    if (!_sdReady || slot >= 8) return DEFAULT_BPM;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return DEFAULT_BPM;
    ProjectHeader hdr;
    if (!readHeader(file, hdr)) { file.close(); return DEFAULT_BPM; }
    file.close();
    return hdr.bpm;
}

void Storage::loadProjectName(uint8_t slot, char* buf, uint8_t len) {
    buf[0] = '\0';
    if (!_sdReady || slot >= 8) return;
    char path[48];
    projectPath(slot, path, sizeof(path));
    File file = SD.open(path);
    if (!file) return;
    ProjectHeader hdr;
    if (!readHeader(file, hdr)) { file.close(); return; }
    file.close();
    strncpy(buf, hdr.name, len - 1);
    buf[len - 1] = '\0';
}

bool Storage::saveProject(const Project& project, uint8_t slot) {
    if (!_sdReady || slot >= 8) return false;

    char dir[32];
    projectDir(slot, dir, sizeof(dir));
    SD.mkdir("/beepbotdx");
    SD.mkdir(dir);

    // Save each occupied sound as WAV
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (project.sounds[i].occupied) {
            char path[64];
            snprintf(path, sizeof(path), "%s/s%d.wav", dir, i);
            saveWav(project.sounds[i], path);
        }
    }

    // Save project metadata
    char datPath[48];
    projectPath(slot, datPath, sizeof(datPath));
    File file = SD.open(datPath, FILE_WRITE);
    if (!file) return false;

    ProjectHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = PROJECT_MAGIC;
    hdr.version = PROJECT_VERSION;
    hdr.bpm = project.bpm;
    hdr.themeIndex = project.themeIndex;
    hdr.bitDepth = (uint8_t)project.bitDepth;
    strncpy(hdr.name, project.name, 8);
    hdr.name[8] = '\0';
    memcpy(hdr.patterns, project.patterns, sizeof(hdr.patterns));
    memcpy(hdr.song, project.song, sizeof(hdr.song));

    for (int i = 0; i < NUM_SOUNDS; i++) {
        hdr.soundOccupied[i] = project.sounds[i].occupied ? 1 : 0;
        hdr.soundLevels[i] = project.sounds[i].level;
        if (project.sounds[i].occupied) {
            strncpy(hdr.soundNames[i], project.sounds[i].name, 8);
            hdr.soundNames[i][8] = '\0';
        }
        memcpy(hdr.fxValues[i], project.sounds[i].fx.value, NUM_FX);
        uint8_t bits = 0;
        for (int f2 = 0; f2 < NUM_FX; f2++) {
            if (project.sounds[i].fx.enabled[f2]) bits |= (1 << f2);
        }
        hdr.fxEnabled[i] = bits;
    }

    file.write((const uint8_t*)&hdr, sizeof(hdr));
    file.close();

    Serial.printf("[project] saved to slot %d\n", slot + 1);
    return true;
}

bool Storage::loadProject(Project& project, uint8_t slot) {
    if (!_sdReady || slot >= 8) return false;

    char datPath[48];
    projectPath(slot, datPath, sizeof(datPath));
    File file = SD.open(datPath);
    if (!file) return false;

    ProjectHeader hdr;
    if (!readHeader(file, hdr)) {
        file.close();
        Serial.println("[project] bad magic/version");
        return false;
    }
    file.close();

    project.bpm = hdr.bpm;
    project.themeIndex = hdr.themeIndex;
    project.bitDepth = (BitDepth)hdr.bitDepth;
    strncpy(project.name, hdr.name, 8);
    project.name[8] = '\0';
    memcpy(project.patterns, hdr.patterns, sizeof(project.patterns));
    memcpy(project.song, hdr.song, sizeof(project.song));

    char dir[32];
    projectDir(slot, dir, sizeof(dir));

    // Load sounds from WAV files
    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(project.sounds[i]);
        if (hdr.soundOccupied[i]) {
            char path[64];
            snprintf(path, sizeof(path), "%s/s%d.wav", dir, i);
            if (Storage::loadWav(project.sounds[i], path)) {
                SoundSlotOps::setName(project.sounds[i], hdr.soundNames[i]);
                project.sounds[i].level = hdr.soundLevels[i];
            }
        }
        if (hdr.version >= 2) {
            memcpy(project.sounds[i].fx.value, hdr.fxValues[i], NUM_FX);
            for (int f2 = 0; f2 < NUM_FX; f2++) {
                project.sounds[i].fx.enabled[f2] = (hdr.fxEnabled[i] & (1 << f2)) != 0;
            }
        } else {
            SlotFxOps::defaults(project.sounds[i].fx);
        }
    }

    Serial.printf("[project] loaded from slot %d\n", slot + 1);
    return true;
}

bool Storage::renderSongToWav(const Project& project, const char* path) {
    if (!_sdReady) return false;

    SD.mkdir("/beepbotdx");

    uint32_t samplesPerStep = SAMPLE_RATE * 60 / project.bpm / 4;

    uint8_t songLength = 0;
    for (uint8_t i = 0; i < NUM_SONG_POSITIONS; i++) {
        if (project.song[i] < NUM_PATTERNS) songLength = i + 1;
    }
    if (songLength == 0) songLength = 1;

    uint32_t totalSteps = songLength * NUM_STEPS;
    uint32_t totalSamples = totalSteps * samplesPerStep;

    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;

    uint32_t dataSize = totalSamples * sizeof(int16_t);
    uint32_t fileSize = 36 + dataSize;

    // Write WAV header
    file.write((const uint8_t*)"RIFF", 4);
    file.write((const uint8_t*)&fileSize, 4);
    file.write((const uint8_t*)"WAVE", 4);
    file.write((const uint8_t*)"fmt ", 4);
    uint32_t fmtSize = 16;
    file.write((const uint8_t*)&fmtSize, 4);
    uint16_t audioFormat = 1;
    file.write((const uint8_t*)&audioFormat, 2);
    uint16_t numChannels = 1;
    file.write((const uint8_t*)&numChannels, 2);
    uint32_t sampleRate = SAMPLE_RATE;
    file.write((const uint8_t*)&sampleRate, 4);
    uint32_t byteRate = SAMPLE_RATE * 2;
    file.write((const uint8_t*)&byteRate, 4);
    uint16_t blockAlign = 2;
    file.write((const uint8_t*)&blockAlign, 2);
    uint16_t bitsPerSample = 16;
    file.write((const uint8_t*)&bitsPerSample, 2);
    file.write((const uint8_t*)"data", 4);
    file.write((const uint8_t*)&dataSize, 4);

    // Voice state for mixing
    struct Voice {
        const int16_t* samples;
        uint32_t length;
        uint32_t srcRate;
        uint8_t level;
        bool active;
        SlotFx fx;
        FxFilterState filterState;
        double fracPos;
    };
    Voice voices[NUM_VOICES];
    memset(voices, 0, sizeof(voices));
    uint8_t nextVoice = 0;

    // Render chunk by chunk
    const uint32_t CHUNK = 512;
    int16_t chunk[CHUNK];

    uint32_t samplePos = 0;
    for (uint32_t step = 0; step < totalSteps; step++) {
        uint8_t songPos = step / NUM_STEPS;
        uint8_t patStep = step % NUM_STEPS;
        uint8_t patIdx = project.song[songPos];
        uint8_t triggers = (patIdx < NUM_PATTERNS) ? project.patterns[patIdx].steps[patStep] : 0;

        // Fire triggers
        for (uint8_t s = 0; s < NUM_SOUNDS; s++) {
            if ((triggers & (1 << s)) && project.sounds[s].occupied) {
                Voice& v = voices[nextVoice];
                v.samples = project.sounds[s].samples;
                v.length = project.sounds[s].length;
                v.srcRate = project.sounds[s].sampleRate;
                v.level = project.sounds[s].level;
                v.fracPos = 0.0;
                v.fx = project.sounds[s].fx;
                FxDsp::initFilterState(v.filterState);
                v.active = true;
                nextVoice = (nextVoice + 1) % NUM_VOICES;
            }
        }

        // Render this step's samples
        uint32_t stepEnd = samplePos + samplesPerStep;
        while (samplePos < stepEnd) {
            uint32_t toRender = stepEnd - samplePos;
            if (toRender > CHUNK) toRender = CHUNK;

            memset(chunk, 0, toRender * sizeof(int16_t));

            for (int v = 0; v < NUM_VOICES; v++) {
                if (!voices[v].active) continue;
                double step = (double)voices[v].srcRate / SAMPLE_RATE;
                if (voices[v].fx.enabled[FX_PITCH])
                    step *= FxDsp::pitchRate(voices[v].fx.value[FX_PITCH]);
                for (uint32_t i = 0; i < toRender; i++) {
                    uint32_t idx = (uint32_t)voices[v].fracPos;
                    if (idx >= voices[v].length) {
                        voices[v].active = false;
                        break;
                    }
                    double frac = voices[v].fracPos - idx;
                    int16_t s;
                    if (idx + 1 < voices[v].length)
                        s = (int16_t)(voices[v].samples[idx] * (1.0 - frac) + voices[v].samples[idx + 1] * frac);
                    else
                        s = voices[v].samples[idx];
                    s = FxDsp::processSample(s, voices[v].fx.value, voices[v].fx.enabled, voices[v].filterState, (float)voices[v].srcRate);
                    int32_t sample = (int32_t)s * voices[v].level / 100;
                    int32_t mixed = (int32_t)chunk[i] + sample;
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    chunk[i] = (int16_t)mixed;
                    voices[v].fracPos += step;
                }
            }

            file.write((const uint8_t*)chunk, toRender * sizeof(int16_t));
            samplePos += toRender;
        }
    }

    file.close();
    Serial.printf("[render] exported: %s (%u samples)\n", path, totalSamples);
    return true;
}
