#pragma once

#include <cstdint>
#include "config.h"
#include "project.h"

enum Density { DENSITY_SPARSE, DENSITY_MEDIUM, DENSITY_BUSY };
enum GrooveFamily { GROOVE_STRAIGHT, GROOVE_SYNCOPATED, GROOVE_DRIVING, GROOVE_BROKEN, GROOVE_MINIMAL };
enum PhraseStructure { PHRASE_AAAB, PHRASE_ABAB, PHRASE_AABB, PHRASE_ABAC };

struct GenResult {
    uint8_t steps[NUM_STEPS];
    Density density;
    GrooveFamily groove;
    const char* message;
};

namespace PatternGen {
    void init();
    GenResult generate(const Project& project);
}
