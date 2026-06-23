#include "character.h"
#include "timing.h"
#include <cstring>

void Character::setState(CharacterState state) {
    if (state == CHAR_BEAT) {
        _prevState = _state;
    }
    _state = state;
    _stateTime = millis();
}

void Character::say(const char* msg) {
    strncpy(_message, msg, 15);
    _message[15] = '\0';
    _msgTime = millis();
}

CharacterState Character::getState() const {
    return _state;
}

const char* Character::getFace() const {
    switch (_state) {
        case CHAR_IDLE:      return "(^_^)";
        case CHAR_FOCUSED:   return "(o_o)";
        case CHAR_RECORDING: return "(*O*)";
        case CHAR_PLAYING:   return "(>_<)b";
        case CHAR_SAVING:    return "(^_^;)";
        case CHAR_ERROR:     return "(>_<)";
        case CHAR_SUCCESS:   return "\\(^o^)/";
        case CHAR_BEAT:      return "(^O^)";
        case CHAR_FLIP:      return "(╯°□°)╯︵ ♪♫♬";
        case CHAR_FLIP_BACK: return "♪♫♬ノ(°_°ノ)";
    }
    return "(^_^)";
}

const char* Character::getMessage() const {
    return _message;
}

bool Character::hasMessage() const {
    return _message[0] != '\0';
}

void Character::tick() {
    if (_state == CHAR_BEAT) {
        uint32_t elapsed = millis() - _stateTime;
        if (elapsed > 100) {
            _state = _prevState;
        }
    }
    if (_state == CHAR_SUCCESS || _state == CHAR_ERROR || _state == CHAR_FLIP_BACK) {
        uint32_t elapsed = millis() - _stateTime;
        if (elapsed > 1500) {
            _state = CHAR_IDLE;
        }
    }
    if (_state == CHAR_FLIP) {
        uint32_t elapsed = millis() - _stateTime;
        if (elapsed > 1500) {
            _state = CHAR_FLIP_BACK;
            _stateTime = millis();
        }
    }
    if (_message[0] && (millis() - _msgTime > 1200)) {
        _message[0] = '\0';
    }
}
