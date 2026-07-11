#include "project.h"
#include <cstring>

void Project::init(Project& p) {
    p.bpm = DEFAULT_BPM;
    p.themeIndex = 0;
    p.name[0] = '\0';

    for (int i = 0; i < NUM_SOUNDS; i++) {
        SoundSlotOps::free(p.sounds[i]);
    }

    memset(p.patterns, 0, sizeof(p.patterns));
    memset(p.song, 0xFF, sizeof(p.song));
    p.song[0] = 0;
    p.dirty = false;
}
