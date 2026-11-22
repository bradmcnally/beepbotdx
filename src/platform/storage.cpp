#include "core/project_format.h"
#include "core/wav.h"
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
#include <cmath>

#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#endif

static bool _sdReady = false;

class WavFile : public Wav::Stream {
    File& file;
public:
    explicit WavFile(File& f) : file(f) {}
    size_t read(void* p, size_t n) override { return file.read((uint8_t*)p, n); }
    size_t write(const void* p, size_t n) override { return file.write((const uint8_t*)p, n); }
    uint32_t tell() override { return file.position(); }
    bool seek(uint32_t p) override { return file.seek(p); }
};

bool Storage::init() {
    SPI.begin(SD_SPI_CLK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
    _sdReady = SD.begin(SD_SPI_CS, SPI);
    Serial.printf("[storage] SD init: %s\n", _sdReady ? "OK" : "FAIL");
    return _sdReady;
}

bool Storage::isReady() {
    return _sdReady;
}

bool Storage::loadWav(SoundSlot& slot, const char* path, BitDepth targetBitDepth) {
    File file = SD.open(path);
    if (!file) return false;
    WavFile stream(file);
    bool ok = Wav::load(stream, slot, path, targetBitDepth);
    file.close();
    return ok;
}

bool Storage::saveWav(const SoundSlot& slot, const char* path) {
    if (!slot.samples || slot.length == 0) return false;
    SD.mkdir("/beepbotdx");
    SD.mkdir("/beepbotdx/samples");
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    WavFile stream(file);
    bool ok = Wav::save(stream, slot);
    file.close();
    return ok;
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

    // Reverse so newest files appear first
    for (int i = 0; i < count / 2; i++) {
        char tmp[32];
        memcpy(tmp, names[i], 32);
        memcpy(names[i], names[count - 1 - i], 32);
        memcpy(names[count - 1 - i], tmp, 32);
    }

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
    WavFile stream(file);
    return ProjectFormat::read(stream, file.size(), hdr);
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
    bool written = file.write((const uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr);
    file.close();
    return written;
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
    bool written = file.write((const uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr);
    file.close();
    return written;
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
            if (!saveWav(project.sounds[i], path)) return false;
        }
    }

    // Save project metadata
    char datPath[48];
    projectPath(slot, datPath, sizeof(datPath));
    File file = SD.open(datPath, FILE_WRITE);
    if (!file) return false;

    ProjectHeader hdr;
    ProjectFormat::encode(project, hdr);

    if (file.write((const uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr)) {
        file.close();
        return false;
    }
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

    ProjectFormat::apply(hdr, project);

    char dir[32];
    projectDir(slot, dir, sizeof(dir));

    // Load sounds from WAV files
    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(project.sounds[i]);
        if (hdr.soundOccupied[i]) {
            char path[64];
            snprintf(path, sizeof(path), "%s/s%d.wav", dir, i);
            BitDepth slotDepth = hdr.version >= 4
                ? (BitDepth)hdr.soundBitDepth[i] : BIT_DEPTH_16;
            if (Storage::loadWav(project.sounds[i], path, slotDepth)) {
                SoundSlotOps::setName(project.sounds[i], hdr.soundNames[i]);
                project.sounds[i].level = hdr.soundLevels[i];
            }
        }
        ProjectFormat::applySoundFx(hdr, i, project.sounds[i]);
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
        const SoundSlot* slot;
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
                v.slot = &project.sounds[s];
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
                    if (idx >= voices[v].slot->length) {
                        voices[v].active = false;
                        break;
                    }
                    double frac = voices[v].fracPos - idx;
                    int16_t s;
                    if (idx + 1 < voices[v].slot->length)
                        s = (int16_t)(
                            SoundSlotOps::getPlaybackSample(*voices[v].slot, idx) * (1.0 - frac) +
                            SoundSlotOps::getPlaybackSample(*voices[v].slot, idx + 1) * frac);
                    else
                        s = SoundSlotOps::getPlaybackSample(*voices[v].slot, idx);
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
