#include "sequencer.h"

void Sequencer::init(Project* project) {
    _project = project;
    _mode = SEQ_STOPPED;
    _step = 0;
    _patternIndex = 0;
    _songPosition = 0;
}

void Sequencer::setCallback(TriggerCallback cb) {
    _callback = cb;
}

void Sequencer::playPattern(uint8_t patternIndex, bool oneShot) {
    _patternIndex = patternIndex;
    _step = 0;
    _mode = SEQ_PATTERN;
    _lastStepTime = 0;
    _oneShot = oneShot;
}

void Sequencer::playSong(uint8_t startPosition) {
    _songPosition = startPosition;
    uint8_t start = _songPosition;
    while (_project->song[_songPosition] == 0xFF) {
        _songPosition = (_songPosition + 1) % NUM_SONG_POSITIONS;
        if (_songPosition == start) return;
    }
    _patternIndex = _project->song[_songPosition];
    _step = 0;
    _mode = SEQ_SONG;
    _lastStepTime = 0;
    _oneShot = false;
}

void Sequencer::stop() {
    _mode = SEQ_STOPPED;
}

uint32_t Sequencer::stepIntervalMs() const {
    if (!_project) return 125;
    return 60000 / _project->bpm / 4;
}

void Sequencer::fireStep() {
    if (!_callback) return;
    uint8_t triggers = _project->patterns[_patternIndex].steps[_step];
    for (uint8_t i = 0; i < NUM_SOUNDS; i++) {
        if (triggers & (1 << i)) {
            _callback(i);
        }
    }
}

void Sequencer::tick(uint32_t now) {
    if (_mode == SEQ_STOPPED || !_project) return;

    if (_lastStepTime == 0) {
        _lastStepTime = now;
        fireStep();
        return;
    }

    uint32_t interval = stepIntervalMs();
    while (now - _lastStepTime >= interval) {
        _lastStepTime += interval;
        _step++;

        if (_step >= NUM_STEPS) {
            if (_oneShot) {
                stop();
                return;
            }
            _step = 0;
            if (_mode == SEQ_SONG) {
                uint8_t start = _songPosition;
                do {
                    _songPosition = (_songPosition + 1) % NUM_SONG_POSITIONS;
                } while (_project->song[_songPosition] == 0xFF && _songPosition != start);
                if (_project->song[_songPosition] == 0xFF) {
                    stop();
                    return;
                }
                _patternIndex = _project->song[_songPosition];
            }
        }

        fireStep();
    }
}
