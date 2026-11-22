#include "core/project_format.h"
#include "core/wav.h"
#include "platform/storage.h"
#include "storage_paths.h"
#include "core/project_header.h"
#include "core/fx_dsp.h"
#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>

static bool _ready = false;

class WavFile : public Wav::Stream {
    FILE* file;
public:
    explicit WavFile(FILE* f) : file(f) {}
    size_t read(void* p, size_t n) override { return fread(p, 1, n, file); }
    size_t write(const void* p, size_t n) override { return fwrite(p, 1, n, file); }
    uint32_t tell() override { return (uint32_t)ftell(file); }
    bool seek(uint32_t p) override { return fseek(file, p, SEEK_SET) == 0; }
};

const std::filesystem::path& Storage::dataDirectory() {
    static const auto directory = []() -> std::filesystem::path {
#ifdef CARDPUTER_ZERO_DEVICE
        if (const char* overrideDir = std::getenv("BEEPBOTDX_DATA_DIR"); overrideDir && *overrideDir)
            return overrideDir;
        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
            return std::filesystem::path(xdg) / "beepbotdx";
        if (const char* home = std::getenv("HOME"); home && *home)
            return std::filesystem::path(home) / ".local/share/beepbotdx";
#endif
        return "beepbotdx_data";
    }();
    return directory;
}

std::filesystem::path Storage::sampleDirectory() {
#ifdef CARDPUTER_ZERO_DEVICE
    if (const char* overrideDir = std::getenv("BEEPBOTDX_DATA_DIR"); overrideDir && *overrideDir)
        return std::filesystem::path(overrideDir) / "samples";
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / "Music/beepbotdx/samples";
#endif
    return Storage::dataDirectory() / "samples";
}

static std::filesystem::path storagePath(const char* path) {
    constexpr char samplePrefix[] = "/beepbotdx/samples";
    if (strncmp(path, samplePrefix, sizeof(samplePrefix) - 1) == 0 &&
        (path[sizeof(samplePrefix) - 1] == '/' || path[sizeof(samplePrefix) - 1] == '\0')) {
        return Storage::sampleDirectory() / std::string(path + sizeof(samplePrefix) - 1 +
            (path[sizeof(samplePrefix) - 1] == '/' ? 1 : 0));
    }
    constexpr char prefix[] = "/beepbotdx/";
    if (strncmp(path, prefix, sizeof(prefix) - 1) == 0) {
        return Storage::dataDirectory() / std::string(path + sizeof(prefix) - 1);
    }
    return path;
}

bool Storage::init() {
    std::error_code error;
    std::filesystem::create_directories(dataDirectory(), error);
    if (error) return false;
    std::filesystem::create_directories(sampleDirectory(), error);
    _ready = !error;
    return _ready;
}

bool Storage::isReady() {
    return _ready;
}

bool Storage::loadWav(SoundSlot& slot, const char* path, BitDepth targetBitDepth) {
    FILE* file = fopen(storagePath(path).c_str(), "rb");
    if (!file) return false;
    WavFile stream(file);
    bool ok = Wav::load(stream, slot, path, targetBitDepth);
    fclose(file);
    return ok;
}

bool Storage::saveWav(const SoundSlot& slot, const char* path) {
    if (!slot.samples || slot.length == 0) return false;
    FILE* file = fopen(storagePath(path).c_str(), "wb");
    if (!file) return false;
    WavFile stream(file);
    bool ok = Wav::save(stream, slot);
    if (ferror(file) || fflush(file) != 0) ok = false;
    if (fclose(file) != 0) ok = false;
    return ok;
}

bool Storage::listWavFiles(const char* dir, char names[][32], uint8_t& count, uint8_t max) {
    DIR* d = opendir(storagePath(dir).c_str());
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

static std::filesystem::path projectDir(uint8_t slot) {
    char name[4];
    snprintf(name, sizeof(name), "%02u", static_cast<unsigned>(slot) + 1);
    return Storage::dataDirectory() / name;
}

static std::filesystem::path projectPath(uint8_t slot) {
    return projectDir(slot) / "project.dat";
}

bool Storage::projectExists(uint8_t slot) {
    if (!_ready || slot >= 8) return false;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (f) { fclose(f); return true; }
    return false;
}

bool Storage::deleteProject(uint8_t slot) {
    if (!_ready || slot >= 8) return false;
    const auto path = projectPath(slot);
    return remove(path.c_str()) == 0;
}

static bool readHeader(FILE* file, ProjectHeader& hdr) {
    if (fseek(file, 0, SEEK_END) != 0) return false;
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) return false;
    WavFile stream(file);
    return ProjectFormat::read(stream, static_cast<size_t>(size), hdr);
}

uint8_t Storage::loadProjectTheme(uint8_t slot) {
    if (!_ready || slot >= 8) return 0;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return 0;
    return hdr.themeIndex;
}

bool Storage::saveProjectTheme(uint8_t slot, uint8_t themeIndex) {
    if (!_ready || slot >= 8) return false;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return false;
    hdr.themeIndex = themeIndex;
    hdr.version = PROJECT_VERSION;
    f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool written = fwrite(&hdr, sizeof(hdr), 1, f) == 1 && fflush(f) == 0;
    if (fclose(f) != 0) written = false;
    return written;
}

bool Storage::saveProjectName(uint8_t slot, const char* name) {
    if (!_ready || slot >= 8) return false;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return false;
    strncpy(hdr.name, name, 8);
    hdr.name[8] = '\0';
    hdr.version = PROJECT_VERSION;
    f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool written = fwrite(&hdr, sizeof(hdr), 1, f) == 1 && fflush(f) == 0;
    if (fclose(f) != 0) written = false;
    return written;
}

uint16_t Storage::loadProjectBpm(uint8_t slot) {
    if (!_ready || slot >= 8) return DEFAULT_BPM;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return DEFAULT_BPM;
    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return DEFAULT_BPM;
    return hdr.bpm;
}

void Storage::loadProjectName(uint8_t slot, char* buf, uint8_t len) {
    buf[0] = '\0';
    if (!_ready || slot >= 8) return;
    const auto path = projectPath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;
    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return;
    strncpy(buf, hdr.name, len - 1);
    buf[len - 1] = '\0';
}

bool Storage::saveProject(const Project& project, uint8_t slot) {
    if (!_ready || slot >= 8) return false;

    const auto dir = projectDir(slot);
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    if (error) return false;

    // Save each occupied sound as WAV
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (project.sounds[i].occupied) {
            const auto path = dir / ("s" + std::to_string(i) + ".wav");
            if (!Storage::saveWav(project.sounds[i], path.c_str())) return false;
        }
    }

    // Save project metadata
    const auto datPath = projectPath(slot);
    FILE* f = fopen(datPath.c_str(), "wb");
    if (!f) return false;

    ProjectHeader hdr;
    ProjectFormat::encode(project, hdr);

    bool written = fwrite(&hdr, sizeof(hdr), 1, f) == 1 &&
                   ferror(f) == 0 && fflush(f) == 0;
    if (fclose(f) != 0) written = false;
    return written;
}

bool Storage::loadProject(Project& project, uint8_t slot) {
    if (!_ready || slot >= 8) return false;

    const auto datPath = projectPath(slot);
    FILE* f = fopen(datPath.c_str(), "rb");
    if (!f) return false;

    ProjectHeader hdr;
    bool valid = readHeader(f, hdr);
    fclose(f);
    if (!valid) return false;

    ProjectFormat::apply(hdr, project);

    const auto dir = projectDir(slot);

    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(project.sounds[i]);
        if (hdr.soundOccupied[i]) {
            const auto path = dir / ("s" + std::to_string(i) + ".wav");
            FILE* wf = fopen(path.c_str(), "rb");
            if (wf) {
                fclose(wf);
                BitDepth slotDepth = hdr.version >= 4
                    ? (BitDepth)hdr.soundBitDepth[i] : BIT_DEPTH_16;
                Storage::loadWav(project.sounds[i], path.c_str(), slotDepth);
                SoundSlotOps::setName(project.sounds[i], hdr.soundNames[i]);
                project.sounds[i].level = hdr.soundLevels[i];
            }
        }
        ProjectFormat::applySoundFx(hdr, i, project.sounds[i]);
    }

    return true;
}

bool Storage::renderSongToWav(const Project& project, const char* path) {
    uint32_t samplesPerStep = SAMPLE_RATE * 60 / project.bpm / 4;

    uint8_t songLength = 0;
    for (uint8_t i = 0; i < NUM_SONG_POSITIONS; i++) {
        if (project.song[i] < NUM_PATTERNS) songLength = i + 1;
    }
    if (songLength == 0) songLength = 1;

    uint32_t totalSteps = songLength * NUM_STEPS;
    uint32_t totalSamples = totalSteps * samplesPerStep;

    FILE* f = fopen(storagePath(path).c_str(), "wb");
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

            fwrite(chunk, 2, toRender, f);
            samplePos += toRender;
        }
    }

    fclose(f);
    return true;
}
