#include "platform/storage.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

static const char* BASE_DIR = "beepbotdx_data";
static bool _ready = false;

static void ensureDir(const char* path) {
    mkdir(path, 0755);
}

bool Storage::init() {
    ensureDir(BASE_DIR);
    char samples[64];
    snprintf(samples, sizeof(samples), "%s/samples", BASE_DIR);
    ensureDir(samples);
    _ready = true;
    return true;
}

bool Storage::isReady() {
    return _ready;
}

bool Storage::loadWav(SoundSlot& slot, const char* path) {
    char fullPath[128];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + strlen("/beepbotdx/"));

    FILE* f = fopen(fullPath, "rb");
    if (!f) {
        // Try path as-is
        f = fopen(path, "rb");
        if (!f) return false;
    }

    // Skip WAV header (44 bytes)
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 44, SEEK_SET);

    uint32_t dataSize = fileSize - 44;
    uint32_t numSamples = dataSize / 2;

    SoundSlotOps::free(slot);
    if (!SoundSlotOps::allocate(slot, numSamples)) {
        fclose(f);
        return false;
    }

    fread(slot.samples, 2, numSamples, f);
    fclose(f);

    slot.length = numSamples;
    slot.sampleRate = SAMPLE_RATE;
    slot.occupied = true;

    // Extract name from filename
    const char* name = strrchr(path, '/');
    name = name ? name + 1 : path;
    char nameBuf[9];
    strncpy(nameBuf, name, 8);
    nameBuf[8] = '\0';
    char* dot = strrchr(nameBuf, '.');
    if (dot) *dot = '\0';
    SoundSlotOps::setName(slot, nameBuf);

    return true;
}

bool Storage::saveWav(const SoundSlot& slot, const char* path) {
    char fullPath[128];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + strlen("/beepbotdx/"));

    FILE* f = fopen(fullPath, "wb");
    if (!f) return false;

    uint32_t dataSize = slot.length * 2;
    uint32_t fileSize = dataSize + 36;

    // WAV header
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 4, 1, f);
    uint16_t audioFmt = 1;
    fwrite(&audioFmt, 2, 1, f);
    uint16_t channels = 1;
    fwrite(&channels, 2, 1, f);
    uint32_t sampleRate = slot.sampleRate;
    fwrite(&sampleRate, 4, 1, f);
    uint32_t byteRate = sampleRate * 2;
    fwrite(&byteRate, 4, 1, f);
    uint16_t blockAlign = 2;
    fwrite(&blockAlign, 2, 1, f);
    uint16_t bitsPerSample = 16;
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(slot.samples, 2, slot.length, f);
    fclose(f);

    return true;
}

bool Storage::listWavFiles(const char* dir, char names[][32], uint8_t& count, uint8_t max) {
    char fullPath[128];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, dir + strlen("/beepbotdx/"));

    DIR* d = opendir(fullPath);
    if (!d) return false;

    count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr && count < max) {
        const char* name = entry->d_name;
        int len = strlen(name);
        if (len > 4 && strcasecmp(name + len - 4, ".wav") == 0) {
            strncpy(names[count], name, 31);
            names[count][31] = '\0';
            count++;
        }
    }
    closedir(d);
    return count > 0;
}

bool Storage::saveProject(const Project& project, uint8_t slot) {
    char dir[64];
    snprintf(dir, sizeof(dir), "%s/p%d", BASE_DIR, slot + 1);
    ensureDir(dir);

    char path[80];
    snprintf(path, sizeof(path), "%s/project.bin", dir);
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    uint8_t version = 2;
    fwrite(&version, 1, 1, f);
    fwrite(&project.bpm, 2, 1, f);
    fwrite(&project.themeIndex, 1, 1, f);

    for (int i = 0; i < NUM_PATTERNS; i++) {
        fwrite(project.patterns[i].steps, 1, NUM_STEPS, f);
    }
    fwrite(project.song, 1, NUM_SONG_POSITIONS, f);

    // Save sounds
    for (int i = 0; i < NUM_SOUNDS; i++) {
        fwrite(&project.sounds[i].occupied, 1, 1, f);
        if (project.sounds[i].occupied) {
            fwrite(project.sounds[i].name, 1, 9, f);
            fwrite(&project.sounds[i].length, 4, 1, f);
            fwrite(&project.sounds[i].sampleRate, 4, 1, f);
            fwrite(project.sounds[i].samples, 2, project.sounds[i].length, f);
        }
    }

    fclose(f);
    return true;
}

bool Storage::loadProject(Project& project, uint8_t slot) {
    char path[80];
    snprintf(path, sizeof(path), "%s/p%d/project.bin", BASE_DIR, slot + 1);
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint8_t version;
    fread(&version, 1, 1, f);
    if (version != 2) { fclose(f); return false; }

    fread(&project.bpm, 2, 1, f);
    fread(&project.themeIndex, 1, 1, f);

    for (int i = 0; i < NUM_PATTERNS; i++) {
        fread(project.patterns[i].steps, 1, NUM_STEPS, f);
    }
    fread(project.song, 1, NUM_SONG_POSITIONS, f);

    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(project.sounds[i]);
        uint8_t occupied;
        fread(&occupied, 1, 1, f);
        if (occupied) {
            fread(project.sounds[i].name, 1, 9, f);
            uint32_t length, sampleRate;
            fread(&length, 4, 1, f);
            fread(&sampleRate, 4, 1, f);
            if (SoundSlotOps::allocate(project.sounds[i], length)) {
                fread(project.sounds[i].samples, 2, length, f);
                project.sounds[i].length = length;
                project.sounds[i].sampleRate = sampleRate;
                project.sounds[i].occupied = true;
            }
        }
    }

    fclose(f);
    return true;
}

bool Storage::projectExists(uint8_t slot) {
    char path[80];
    snprintf(path, sizeof(path), "%s/p%d/project.bin", BASE_DIR, slot + 1);
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

bool Storage::renderSongToWav(const Project& project, const char* path) {
    char fullPath[128];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + strlen("/beepbotdx/"));
    // Simplified: write silence as placeholder
    FILE* f = fopen(fullPath, "wb");
    if (!f) return false;

    uint32_t numSamples = SAMPLE_RATE * 10;
    uint32_t dataSize = numSamples * 2;
    uint32_t fileSize = dataSize + 36;

    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16; fwrite(&fmtSize, 4, 1, f);
    uint16_t audioFmt = 1; fwrite(&audioFmt, 2, 1, f);
    uint16_t channels = 1; fwrite(&channels, 2, 1, f);
    uint32_t sr = SAMPLE_RATE; fwrite(&sr, 4, 1, f);
    uint32_t byteRate = sr * 2; fwrite(&byteRate, 4, 1, f);
    uint16_t blockAlign = 2; fwrite(&blockAlign, 2, 1, f);
    uint16_t bps = 16; fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);

    int16_t silence[512] = {};
    uint32_t written = 0;
    while (written < numSamples) {
        uint32_t chunk = numSamples - written;
        if (chunk > 512) chunk = 512;
        fwrite(silence, 2, chunk, f);
        written += chunk;
    }

    fclose(f);
    return true;
}
