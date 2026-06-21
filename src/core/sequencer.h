#pragma once

#include <cstdint>
#include "config.h"
#include "project.h"

enum SequencerMode {
    SEQ_STOPPED,
    SEQ_PATTERN,
    SEQ_SONG,
};

using TriggerCallback = void(*)(uint8_t soundIndex);

class Sequencer {
public:
    void init(Project* project);
    void setCallback(TriggerCallback cb);

    void playPattern(uint8_t patternIndex, bool oneShot = false);
    void playSong(uint8_t startPosition);
    void stop();
    void tick(uint32_t now);

    SequencerMode getMode() const { return _mode; }
    uint8_t getCurrentStep() const { return _step; }
    uint8_t getCurrentPattern() const { return _patternIndex; }
    uint8_t getCurrentSongPosition() const { return _songPosition; }
    bool isPlaying() const { return _mode != SEQ_STOPPED; }

    uint32_t stepIntervalMs() const;

private:
    void fireStep();

    Project* _project = nullptr;
    TriggerCallback _callback = nullptr;
    SequencerMode _mode = SEQ_STOPPED;
    uint8_t _patternIndex = 0;
    uint8_t _step = 0;
    uint8_t _songPosition = 0;
    uint32_t _lastStepTime = 0;
    bool _oneShot = false;
};
