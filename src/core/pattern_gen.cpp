#include "pattern_gen.h"
#include <cstdlib>
#include <cstring>

static const int HISTORY_SIZE = 8;
static uint8_t history[HISTORY_SIZE][NUM_STEPS];
static int historyCount = 0;
static int historyIndex = 0;

// Anchor weight profiles per groove family [16 positions]
// Higher = more likely to place an anchor hit there
static const uint8_t grooveWeights[5][NUM_STEPS] = {
    // Straight: strong downbeats
    {9, 2, 4, 2, 8, 2, 4, 2, 8, 2, 4, 2, 7, 2, 4, 2},
    // Syncopated: offbeats and unexpected positions
    {4, 3, 6, 7, 3, 5, 7, 4, 3, 6, 5, 7, 4, 5, 7, 3},
    // Driving: heavy on beats with eighth-note push
    {9, 5, 7, 4, 9, 5, 7, 4, 9, 5, 7, 4, 8, 5, 6, 5},
    // Broken: irregular groupings
    {8, 1, 3, 6, 2, 7, 1, 5, 8, 2, 6, 1, 4, 7, 2, 5},
    // Minimal: sparse, strong positions only
    {9, 1, 2, 1, 7, 1, 2, 1, 8, 1, 2, 1, 6, 1, 2, 3},
};

static const char* msgGeneral[] = {"New beat!", "Try this!", "Fresh one!", "Nice!", "Ooh!", "Another!", "Beep!"};
static const char* msgSparse[] = {"Lots of space.", "Keep it simple.", "Minimal!"};
static const char* msgBusy[] = {"Busy one!", "It's packed!"};
static const char* msgSyncopated[] = {"Unexpected!", "Off the grid!"};
static const char* msgStraight[] = {"Locked in!", "Steady!", "Let's go!"};

static int weightedRoll(int* weights, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) total += weights[i];
    int r = rand() % total;
    for (int i = 0; i < count; i++) {
        r -= weights[i];
        if (r < 0) return i;
    }
    return count - 1;
}

static Density chooseDensity() {
    int w[] = {20, 60, 20};
    return (Density)weightedRoll(w, 3);
}

static GrooveFamily chooseGroove() {
    int w[] = {30, 25, 20, 15, 10};
    return (GrooveFamily)weightedRoll(w, 5);
}

static PhraseStructure choosePhrase() {
    int w[] = {30, 30, 20, 20};
    return (PhraseStructure)weightedRoll(w, 4);
}

static int chooseVariationCount() {
    int w[] = {20, 65, 15};
    return weightedRoll(w, 3);
}

static void generateGroup(uint8_t* group, int targetHits, const uint8_t* weights) {
    memset(group, 0, 4);
    int placed = 0;

    // Place hits weighted by groove profile
    int attempts = 0;
    while (placed < targetHits && attempts < 40) {
        int totalW = 0;
        for (int i = 0; i < 4; i++) {
            if (!group[i]) totalW += weights[i];
        }
        if (totalW == 0) break;

        int r = rand() % totalW;
        for (int i = 0; i < 4; i++) {
            if (group[i]) continue;
            r -= weights[i];
            if (r < 0) {
                group[i] = 1;
                placed++;
                break;
            }
        }
        attempts++;
    }
}

static void varyGroup(uint8_t* group) {
    // Small mutation: flip one position
    int pos = rand() % 4;
    group[pos] ^= 1;
}

static void generateSoundLayer(uint8_t* steps, int targetHits, GrooveFamily groove,
                                PhraseStructure phrase, bool isAnchor) {
    const uint8_t* weights = grooveWeights[groove];

    // Determine hits per group (distribute target across 4 groups)
    int basePerGroup = targetHits / 4;
    int remainder = targetHits % 4;

    // Generate group A
    uint8_t groupA[4];
    int hitsA = basePerGroup + (remainder > 0 ? 1 : 0);
    generateGroup(groupA, hitsA, &weights[0]);

    // Generate group B (different from A)
    uint8_t groupB[4];
    int hitsB = basePerGroup + (remainder > 1 ? 1 : 0);
    generateGroup(groupB, hitsB, &weights[4]);

    // Generate group C (for ABAC)
    uint8_t groupC[4];
    int hitsC = basePerGroup + (remainder > 2 ? 1 : 0);
    generateGroup(groupC, hitsC, &weights[8]);

    // Assemble based on phrase structure
    uint8_t groups[4][4];
    switch (phrase) {
        case PHRASE_AAAB:
            memcpy(groups[0], groupA, 4);
            memcpy(groups[1], groupA, 4);
            memcpy(groups[2], groupA, 4);
            memcpy(groups[3], groupB, 4);
            break;
        case PHRASE_ABAB:
            memcpy(groups[0], groupA, 4);
            memcpy(groups[1], groupB, 4);
            memcpy(groups[2], groupA, 4);
            memcpy(groups[3], groupB, 4);
            break;
        case PHRASE_AABB:
            memcpy(groups[0], groupA, 4);
            memcpy(groups[1], groupA, 4);
            memcpy(groups[2], groupB, 4);
            memcpy(groups[3], groupB, 4);
            break;
        case PHRASE_ABAC:
            memcpy(groups[0], groupA, 4);
            memcpy(groups[1], groupB, 4);
            memcpy(groups[2], groupA, 4);
            memcpy(groups[3], groupC, 4);
            break;
    }

    // Final group gets a small variation for fills/interest
    if (rand() % 3 != 0) {
        varyGroup(groups[3]);
    }

    // Flatten into steps
    for (int g = 0; g < 4; g++) {
        for (int s = 0; s < 4; s++) {
            if (groups[g][s]) steps[g * 4 + s] = 1;
        }
    }
}

static void applyVariation(uint8_t* steps, int totalHits) {
    int type = rand() % 5;
    switch (type) {
        case 0: {
            // Pickup: add hit before an existing hit
            for (int i = 1; i < NUM_STEPS; i++) {
                if (steps[i] && !steps[i - 1]) {
                    steps[i - 1] = 1;
                    return;
                }
            }
            break;
        }
        case 1: {
            // Omit an expected hit (remove one non-first hit)
            for (int i = NUM_STEPS - 1; i > 0; i--) {
                if (steps[i]) {
                    steps[i] = 0;
                    return;
                }
            }
            break;
        }
        case 2: {
            // Adjacent pair: double up a hit
            for (int i = 0; i < NUM_STEPS - 1; i++) {
                if (steps[i] && !steps[i + 1]) {
                    steps[i + 1] = 1;
                    return;
                }
            }
            break;
        }
        case 3: {
            // Shift one hit by one step
            for (int i = 1; i < NUM_STEPS - 1; i++) {
                if (steps[i] && !steps[i + 1]) {
                    steps[i] = 0;
                    steps[i + 1] = 1;
                    return;
                }
            }
            break;
        }
        case 4: {
            // Ending fill: add a hit at step 15 or 14
            if (!steps[15]) { steps[15] = 1; return; }
            if (!steps[14]) { steps[14] = 1; return; }
            break;
        }
    }
}

static int countHits(const uint8_t* steps) {
    int count = 0;
    for (int i = 0; i < NUM_STEPS; i++) {
        if (steps[i]) count++;
    }
    return count;
}

static bool validate(const uint8_t patSteps[NUM_STEPS]) {
    // Check total activity
    int totalHits = 0;
    for (int i = 0; i < NUM_STEPS; i++) {
        int bits = 0;
        uint8_t v = patSteps[i];
        while (v) { bits += v & 1; v >>= 1; }
        totalHits += bits;
    }
    if (totalHits == 0) return false;
    if (totalHits < 2) return false;

    // Check consecutive active steps (any sound)
    int consecutive = 0;
    for (int i = 0; i < NUM_STEPS; i++) {
        if (patSteps[i]) { consecutive++; if (consecutive > 6) return false; }
        else consecutive = 0;
    }

    // Check consecutive silent steps
    consecutive = 0;
    for (int i = 0; i < NUM_STEPS; i++) {
        if (!patSteps[i]) { consecutive++; if (consecutive > 8) return false; }
        else consecutive = 0;
    }

    // Check against history
    for (int h = 0; h < historyCount; h++) {
        int diff = 0;
        for (int i = 0; i < NUM_STEPS; i++) {
            if (patSteps[i] != history[h][i]) diff++;
        }
        if (diff < 4) return false;
    }

    return true;
}

static void pushHistory(const uint8_t patSteps[NUM_STEPS]) {
    memcpy(history[historyIndex], patSteps, NUM_STEPS);
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) historyCount++;
}

static const char* chooseMessage(Density density, GrooveFamily groove) {
    // Sometimes pick a density/groove-specific message
    if (rand() % 3 == 0) {
        switch (density) {
            case DENSITY_SPARSE: return msgSparse[rand() % 3];
            case DENSITY_BUSY: return msgBusy[rand() % 2];
            default: break;
        }
    }
    if (rand() % 4 == 0) {
        switch (groove) {
            case GROOVE_SYNCOPATED: return msgSyncopated[rand() % 2];
            case GROOVE_STRAIGHT:
            case GROOVE_DRIVING: return msgStraight[rand() % 3];
            default: break;
        }
    }
    return msgGeneral[rand() % 7];
}

void PatternGen::init() {
    historyCount = 0;
    historyIndex = 0;
}

GenResult PatternGen::generate(const Project& project) {
    GenResult result;
    result.density = DENSITY_MEDIUM;
    result.groove = GROOVE_STRAIGHT;
    memset(result.steps, 0, NUM_STEPS);

    // Count occupied sounds
    int occupiedSounds[NUM_SOUNDS];
    int numOccupied = 0;
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (project.sounds[i].occupied) {
            occupiedSounds[numOccupied++] = i;
        }
    }

    if (numOccupied == 0) {
        result.message = "No sounds!";
        return result;
    }

    // Retry loop for validation
    for (int attempt = 0; attempt < 5; attempt++) {
        memset(result.steps, 0, NUM_STEPS);

        // Musical decisions from seed
        result.density = chooseDensity();
        result.groove = chooseGroove();
        PhraseStructure phrase = choosePhrase();
        int variationCount = chooseVariationCount();

        // Target hit counts per density
        int minHits, maxHits;
        switch (result.density) {
            case DENSITY_SPARSE: minHits = 2; maxHits = 4; break;
            case DENSITY_BUSY:   minHits = 5; maxHits = 8; break;
            default:             minHits = 3; maxHits = 6; break;
        }

        // Generate each sound layer
        for (int idx = 0; idx < numOccupied; idx++) {
            int sound = occupiedSounds[idx];
            bool isAnchor = (idx == 0);

            // Anchor sounds get fewer, stronger hits; texture gets more
            int targetHits;
            if (isAnchor) {
                targetHits = minHits + (rand() % (maxHits - minHits + 1));
            } else {
                // Texture sounds scale with density but vary
                int texMin = (minHits > 1) ? minHits - 1 : 1;
                int texMax = maxHits;
                targetHits = texMin + (rand() % (texMax - texMin + 1));
            }

            // Generate this sound's rhythm
            uint8_t soundSteps[NUM_STEPS];
            memset(soundSteps, 0, NUM_STEPS);
            generateSoundLayer(soundSteps, targetHits, result.groove, phrase, isAnchor);

            // Apply variations to this layer
            for (int v = 0; v < variationCount; v++) {
                applyVariation(soundSteps, countHits(soundSteps));
            }

            // Merge into combined pattern
            for (int i = 0; i < NUM_STEPS; i++) {
                if (soundSteps[i]) {
                    result.steps[i] |= (1 << sound);
                }
            }
        }

        if (validate(result.steps)) {
            pushHistory(result.steps);
            result.message = chooseMessage(result.density, result.groove);
            return result;
        }
    }

    // If all attempts fail, accept the last one anyway
    pushHistory(result.steps);
    result.message = chooseMessage(result.density, result.groove);
    return result;
}
