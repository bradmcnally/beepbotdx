#pragma once

#include "core/sound_slot.h"
#include "core/project.h"

namespace Storage {

bool init();
bool isReady();
bool loadWav(SoundSlot& slot, const char* path);
bool saveWav(const SoundSlot& slot, const char* path);
bool listWavFiles(const char* dir, char names[][32], uint8_t& count, uint8_t max);
bool saveProject(const Project& project, uint8_t slot);
bool loadProject(Project& project, uint8_t slot);
bool projectExists(uint8_t slot);
bool deleteProject(uint8_t slot);
uint8_t loadProjectTheme(uint8_t slot);
bool saveProjectTheme(uint8_t slot, uint8_t themeIndex);
bool saveProjectName(uint8_t slot, const char* name);
uint16_t loadProjectBpm(uint8_t slot);
void loadProjectName(uint8_t slot, char* buf, uint8_t len);
bool renderSongToWav(const Project& project, const char* path);

}
