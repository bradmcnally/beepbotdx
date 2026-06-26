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
    if (strncmp(path, "/beepbotdx/", 11) == 0) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + 11);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s", path);
    }

    FILE* f = fopen(fullPath, "rb");
    if (!f) return false;

    // Read RIFF header
    char riff[4], wave[4];
    uint32_t fileSize;
    if (fread(riff, 1, 4, f) != 4) { fclose(f); return false; }
    fread(&fileSize, 4, 1, f);
    if (fread(wave, 1, 4, f) != 4) { fclose(f); return false; }

    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
        fclose(f);
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
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            long fmtStart = ftell(f);
            fread(&audioFormat, 2, 1, f);
            fread(&numChannels, 2, 1, f);
            fread(&sampleRate, 4, 1, f);
            uint32_t byteRate;
            fread(&byteRate, 4, 1, f);
            uint16_t blockAlign;
            fread(&blockAlign, 2, 1, f);
            fread(&bitsPerSample, 2, 1, f);
            fmtFound = true;
            fseek(f, fmtStart + chunkSize, SEEK_SET);

        } else if (memcmp(chunkId, "data", 4) == 0) {
            if (!fmtFound) { fclose(f); return false; }

            if (audioFormat != 1 && audioFormat != 3) {
                fclose(f);
                return false;
            }

            uint32_t bytesPerSample = bitsPerSample / 8;
            uint32_t numSamples = chunkSize / bytesPerSample / numChannels;
            if (numSamples > MAX_SAMPLE_LENGTH) numSamples = MAX_SAMPLE_LENGTH;

            SoundSlotOps::free(slot);
            if (!SoundSlotOps::allocate(slot, numSamples)) {
                fclose(f);
                return false;
            }

            for (uint32_t i = 0; i < numSamples; i++) {
                int32_t sample = 0;

                if (audioFormat == 1 && bitsPerSample == 8) {
                    uint8_t s;
                    fread(&s, 1, 1, f);
                    sample = ((int16_t)s - 128) << 8;
                    if (numChannels == 2) { uint8_t r; fread(&r, 1, 1, f); sample = (sample + (((int16_t)r - 128) << 8)) / 2; }
                } else if (audioFormat == 1 && bitsPerSample == 16) {
                    int16_t s;
                    fread(&s, 2, 1, f);
                    sample = s;
                    if (numChannels == 2) { int16_t r; fread(&r, 2, 1, f); sample = (sample + r) / 2; }
                } else if (audioFormat == 1 && bitsPerSample == 24) {
                    uint8_t b[3];
                    fread(b, 1, 3, f);
                    sample = (int32_t)((b[2] << 24) | (b[1] << 16) | (b[0] << 8)) >> 8;
                    sample >>= 8;
                    if (numChannels == 2) { fread(b, 1, 3, f); int32_t r = (int32_t)((b[2] << 24) | (b[1] << 16) | (b[0] << 8)) >> 8; r >>= 8; sample = (sample + r) / 2; }
                } else if (audioFormat == 1 && bitsPerSample == 32) {
                    int32_t s;
                    fread(&s, 4, 1, f);
                    sample = s >> 16;
                    if (numChannels == 2) { int32_t r; fread(&r, 4, 1, f); sample = (sample + (r >> 16)) / 2; }
                } else if (audioFormat == 3 && bitsPerSample == 32) {
                    float fl;
                    fread(&fl, 4, 1, f);
                    sample = (int32_t)(fl * 32767.0f);
                    if (numChannels == 2) { float fr; fread(&fr, 4, 1, f); sample = (sample + (int32_t)(fr * 32767.0f)) / 2; }
                } else {
                    SoundSlotOps::free(slot);
                    fclose(f);
                    return false;
                }

                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                slot.samples[i] = (int16_t)sample;
            }

            slot.length = numSamples;
            slot.sampleRate = sampleRate;
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

            fclose(f);
            return true;
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }

    fclose(f);
    return false;
}

bool Storage::saveWav(const SoundSlot& slot, const char* path) {
    if (!slot.samples || slot.length == 0) return false;

    char fullPath[128];
    if (strncmp(path, "/beepbotdx/", 11) == 0) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + 11);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s", path);
    }

    FILE* f = fopen(fullPath, "wb");
    if (!f) return false;

    uint32_t dataSize = slot.length * sizeof(int16_t);
    uint32_t fileSize = 36 + dataSize;

    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16; fwrite(&fmtSize, 4, 1, f);
    uint16_t audioFmt = 1; fwrite(&audioFmt, 2, 1, f);
    uint16_t channels = 1; fwrite(&channels, 2, 1, f);
    uint32_t sr = slot.sampleRate; fwrite(&sr, 4, 1, f);
    uint32_t byteRate = sr * 2; fwrite(&byteRate, 4, 1, f);
    uint16_t blockAlign = 2; fwrite(&blockAlign, 2, 1, f);
    uint16_t bps = 16; fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(slot.samples, 2, slot.length, f);
    fclose(f);

    return true;
}

bool Storage::listWavFiles(const char* dir, char names[][32], uint8_t& count, uint8_t max) {
    char fullPath[128];
    if (strncmp(dir, "/beepbotdx/", 11) == 0) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, dir + 11);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s", dir);
    }

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

static const uint32_t PROJECT_MAGIC = 0x42505844; // "BPXD"
static const uint8_t PROJECT_VERSION = 3;

struct ProjectHeader {
    uint32_t magic;
    uint8_t version;
    uint16_t bpm;
    uint8_t themeIndex;
    uint8_t soundOccupied[NUM_SOUNDS];
    char soundNames[NUM_SOUNDS][9];
    uint8_t soundLevels[NUM_SOUNDS];
    Pattern patterns[NUM_PATTERNS];
    uint8_t song[NUM_SONG_POSITIONS];
};

static void projectDir(uint8_t slot, char* buf, size_t len) {
    snprintf(buf, len, "%s/p%d", BASE_DIR, slot + 1);
}

static void projectPath(uint8_t slot, char* buf, size_t len) {
    snprintf(buf, len, "%s/p%d/project.dat", BASE_DIR, slot + 1);
}

bool Storage::projectExists(uint8_t slot) {
    if (!_ready || slot >= 8) return false;
    char path[80];
    projectPath(slot, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

bool Storage::deleteProject(uint8_t slot) {
    if (!_ready || slot >= 8) return false;
    char path[80];
    projectPath(slot, path, sizeof(path));
    return remove(path) == 0;
}

uint8_t Storage::loadProjectTheme(uint8_t slot) {
    if (!_ready || slot >= 8) return 0;
    char path[80];
    projectPath(slot, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    ProjectHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return 0; }
    fclose(f);
    if (hdr.magic != PROJECT_MAGIC || hdr.version != PROJECT_VERSION) return 0;
    return hdr.themeIndex;
}

uint16_t Storage::loadProjectBpm(uint8_t slot) {
    if (!_ready || slot >= 8) return DEFAULT_BPM;
    char path[80];
    projectPath(slot, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) return DEFAULT_BPM;
    ProjectHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return DEFAULT_BPM; }
    fclose(f);
    if (hdr.magic != PROJECT_MAGIC || hdr.version != PROJECT_VERSION) return DEFAULT_BPM;
    return hdr.bpm;
}

bool Storage::saveProject(const Project& project, uint8_t slot) {
    if (!_ready || slot >= 8) return false;

    char dir[64];
    projectDir(slot, dir, sizeof(dir));
    ensureDir(dir);

    // Save each occupied sound as WAV
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (project.sounds[i].occupied) {
            char path[96];
            snprintf(path, sizeof(path), "%s/s%d.wav", dir, i);
            // Write WAV directly using local path (not /beepbotdx/ prefix)
            const SoundSlot& s = project.sounds[i];
            FILE* f = fopen(path, "wb");
            if (f) {
                uint32_t dataSize = s.length * sizeof(int16_t);
                uint32_t fileSize = 36 + dataSize;
                fwrite("RIFF", 1, 4, f);
                fwrite(&fileSize, 4, 1, f);
                fwrite("WAVE", 1, 4, f);
                fwrite("fmt ", 1, 4, f);
                uint32_t fmtSize = 16; fwrite(&fmtSize, 4, 1, f);
                uint16_t audioFmt = 1; fwrite(&audioFmt, 2, 1, f);
                uint16_t channels = 1; fwrite(&channels, 2, 1, f);
                uint32_t sr = s.sampleRate; fwrite(&sr, 4, 1, f);
                uint32_t byteRate = sr * 2; fwrite(&byteRate, 4, 1, f);
                uint16_t blockAlign = 2; fwrite(&blockAlign, 2, 1, f);
                uint16_t bps = 16; fwrite(&bps, 2, 1, f);
                fwrite("data", 1, 4, f);
                fwrite(&dataSize, 4, 1, f);
                fwrite(s.samples, 2, s.length, f);
                fclose(f);
            }
        }
    }

    // Save project metadata
    char datPath[80];
    projectPath(slot, datPath, sizeof(datPath));
    FILE* f = fopen(datPath, "wb");
    if (!f) return false;

    ProjectHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = PROJECT_MAGIC;
    hdr.version = PROJECT_VERSION;
    hdr.bpm = project.bpm;
    hdr.themeIndex = project.themeIndex;
    memcpy(hdr.patterns, project.patterns, sizeof(hdr.patterns));
    memcpy(hdr.song, project.song, sizeof(hdr.song));

    for (int i = 0; i < NUM_SOUNDS; i++) {
        hdr.soundOccupied[i] = project.sounds[i].occupied ? 1 : 0;
        hdr.soundLevels[i] = project.sounds[i].level;
        if (project.sounds[i].occupied) {
            strncpy(hdr.soundNames[i], project.sounds[i].name, 8);
            hdr.soundNames[i][8] = '\0';
        }
    }

    fwrite(&hdr, sizeof(hdr), 1, f);
    fclose(f);
    return true;
}

bool Storage::loadProject(Project& project, uint8_t slot) {
    if (!_ready || slot >= 8) return false;

    char datPath[80];
    projectPath(slot, datPath, sizeof(datPath));
    FILE* f = fopen(datPath, "rb");
    if (!f) return false;

    ProjectHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return false;
    }
    fclose(f);

    if (hdr.magic != PROJECT_MAGIC || hdr.version != PROJECT_VERSION) {
        return false;
    }

    project.bpm = hdr.bpm;
    project.themeIndex = hdr.themeIndex;
    memcpy(project.patterns, hdr.patterns, sizeof(project.patterns));
    memcpy(project.song, hdr.song, sizeof(project.song));

    char dir[64];
    projectDir(slot, dir, sizeof(dir));

    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(project.sounds[i]);
        if (hdr.soundOccupied[i]) {
            char path[96];
            snprintf(path, sizeof(path), "%s/s%d.wav", dir, i);
            FILE* wf = fopen(path, "rb");
            if (wf) {
                fclose(wf);
                Storage::loadWav(project.sounds[i], path);
                SoundSlotOps::setName(project.sounds[i], hdr.soundNames[i]);
                project.sounds[i].level = hdr.soundLevels[i];
            }
        }
    }

    return true;
}

bool Storage::renderSongToWav(const Project& project, const char* path) {
    char fullPath[128];
    if (strncmp(path, "/beepbotdx/", 11) == 0) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", BASE_DIR, path + 11);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s", path);
    }

    uint32_t samplesPerStep = SAMPLE_RATE * 60 / project.bpm / 4;
    uint32_t totalSteps = NUM_SONG_POSITIONS * NUM_STEPS;
    uint32_t totalSamples = totalSteps * samplesPerStep;

    FILE* f = fopen(fullPath, "wb");
    if (!f) return false;

    uint32_t dataSize = totalSamples * sizeof(int16_t);
    uint32_t fileSize = 36 + dataSize;

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

    struct Voice {
        const int16_t* samples;
        uint32_t length;
        uint32_t pos;
        uint32_t srcRate;
        uint8_t level;
        bool active;
    };
    Voice voices[NUM_VOICES];
    memset(voices, 0, sizeof(voices));
    uint8_t nextVoice = 0;

    const uint32_t CHUNK = 512;
    int16_t chunk[CHUNK];

    uint32_t samplePos = 0;
    for (uint32_t step = 0; step < totalSteps; step++) {
        uint8_t songPos = step / NUM_STEPS;
        uint8_t patStep = step % NUM_STEPS;
        uint8_t patIdx = project.song[songPos];
        uint8_t triggers = (patIdx < NUM_PATTERNS) ? project.patterns[patIdx].steps[patStep] : 0;

        for (uint8_t s = 0; s < NUM_SOUNDS; s++) {
            if ((triggers & (1 << s)) && project.sounds[s].occupied) {
                Voice& v = voices[nextVoice];
                v.samples = project.sounds[s].samples;
                v.length = project.sounds[s].length;
                v.srcRate = project.sounds[s].sampleRate;
                v.level = project.sounds[s].level;
                v.pos = 0;
                v.active = true;
                nextVoice = (nextVoice + 1) % NUM_VOICES;
            }
        }

        uint32_t stepEnd = samplePos + samplesPerStep;
        while (samplePos < stepEnd) {
            uint32_t toRender = stepEnd - samplePos;
            if (toRender > CHUNK) toRender = CHUNK;

            memset(chunk, 0, toRender * sizeof(int16_t));

            for (int v = 0; v < NUM_VOICES; v++) {
                if (!voices[v].active) continue;
                for (uint32_t i = 0; i < toRender; i++) {
                    uint32_t srcPos = (uint32_t)((uint64_t)voices[v].pos * voices[v].srcRate / SAMPLE_RATE);
                    if (srcPos >= voices[v].length) {
                        voices[v].active = false;
                        break;
                    }
                    int32_t sample = (int32_t)voices[v].samples[srcPos] * voices[v].level / 100;
                    int32_t mixed = (int32_t)chunk[i] + sample;
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    chunk[i] = (int16_t)mixed;
                    voices[v].pos++;
                }
            }

            fwrite(chunk, 2, toRender, f);
            samplePos += toRender;
        }
    }

    fclose(f);
    return true;
}
