#include "project_format.h"
#include <cstddef>
#include <cstring>

bool ProjectFormat::read(Wav::Stream& file, size_t size, ProjectHeader& hdr) {
    memset(&hdr, 0, sizeof(hdr));
    const size_t fxSize = sizeof(hdr.fxValues) + sizeof(hdr.fxEnabled);
    const size_t fxOffset = offsetof(ProjectHeader, fxValues);
    if (size < fxOffset) return false;
    if (size <= sizeof(hdr) - fxSize) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&hdr);
        if (file.read(bytes, fxOffset) != fxOffset) return false;
        size_t remaining = size - fxOffset;
        if (file.read(bytes + fxOffset + fxSize, remaining) != remaining) return false;
    } else {
        size_t bytes = size < sizeof(hdr) ? size : sizeof(hdr);
        if (file.read(&hdr, bytes) != bytes) return false;
    }
    return hdr.magic == PROJECT_MAGIC && hdr.version >= 1 && hdr.version <= PROJECT_VERSION;
}

void ProjectFormat::encode(const Project& project, ProjectHeader& hdr) {
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
        hdr.soundBitDepth[i] = (uint8_t)project.sounds[i].bitDepth;
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
        // Bit 4 was unused in the existing header; old projects read as off.
        if (project.sounds[i].normalize) hdr.fxEnabled[i] |= 0x10;
    }

}

void ProjectFormat::apply(const ProjectHeader& hdr, Project& project) {
    project.bpm = hdr.bpm;
    project.themeIndex = hdr.themeIndex;
    project.bitDepth = (BitDepth)hdr.bitDepth;
    strncpy(project.name, hdr.name, 8);
    project.name[8] = '\0';
    memcpy(project.patterns, hdr.patterns, sizeof(project.patterns));
    memcpy(project.song, hdr.song, sizeof(project.song));

}

void ProjectFormat::applySoundFx(const ProjectHeader& hdr, int index, SoundSlot& sound) {
    sound.normalize = hdr.version >= 2 && (hdr.fxEnabled[index] & 0x10) != 0;
    SoundSlotOps::updateNormalizationGain(sound);
    if (hdr.version >= 2) {
        memcpy(sound.fx.value, hdr.fxValues[index], NUM_FX);
        for (int f2 = 0; f2 < NUM_FX; f2++) {
            sound.fx.enabled[f2] = (hdr.fxEnabled[index] & (1 << f2)) != 0;
        }
    } else {
        SlotFxOps::defaults(sound.fx);
    }
}
