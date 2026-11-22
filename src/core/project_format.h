#pragma once
#include "project_header.h"
#include "wav.h"
namespace ProjectFormat {
bool read(Wav::Stream& file, size_t size, ProjectHeader& hdr);
void encode(const Project& project, ProjectHeader& hdr);
void apply(const ProjectHeader& hdr, Project& project);
void applySoundFx(const ProjectHeader& hdr, int index, SoundSlot& sound);
}
