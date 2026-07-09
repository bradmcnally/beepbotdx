#include "character.h"
#include "timing.h"
#include <cstring>

static const uint32_t SLEEP_TIMEOUT = 30000;

void Character::setState(CharacterState state) {
    if (state == CHAR_BEAT || state == CHAR_DANCE_L || state == CHAR_DANCE_R || state == CHAR_JAMMING) {
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

void Character::noteInput() {
    _lastInputTime = millis();
}

void Character::wake() {
    if (_state == CHAR_SLEEPING) {
        _state = CHAR_STARTLED;
        _stateTime = millis();
        say("hey!");
    }
    _lastInputTime = millis();
}

CharacterState Character::getState() const {
    return _state;
}

const char* Character::getFace() const {
    uint32_t elapsed = millis() - _stateTime;

    switch (_state) {
        case CHAR_IDLE:      return "(^_^)";
        case CHAR_FOCUSED:   return "(o_o)";
        case CHAR_RECORDING: {
            uint32_t frame = (elapsed / 500) % 3;
            if (frame == 0) return "(*O*)";
            if (frame == 1) return "(*o*)";
            return "(*O*)";
        }
        case CHAR_SAVING:    return "(^_^;)";
        case CHAR_ERROR:     return "(>_<)";
        case CHAR_SUCCESS:   return "\\(^o^)/";
        case CHAR_BEAT:      return "<(^_^)>";
        case CHAR_FLIP:      return "(╯°□°)╯︵ ♪♫♬♪";
        case CHAR_FLIP_BACK: return "♪♫♬♪ノ(°_°ノ)";
        case CHAR_SLEEPING: {
            uint32_t frame = (elapsed / 600) % 4;
            if (frame == 0) return "(-_-)Zzz";
            if (frame == 1) return "(-_-)zZz";
            if (frame == 2) return "(-_-)zzZ";
            return "(-_-)zzz";
        }
        case CHAR_STARTLED:  return "(O_O)!";
        case CHAR_DANCE_L:   return "<(^_^<)";
        case CHAR_DANCE_R:   return "(>^_^)>";
        case CHAR_JAMMING:   return "\\(>.<)/";
        case CHAR_DIZZY:     return "(@_@)";
        case CHAR_DEAD:      return "(x_x)";
        case CHAR_CRYING:    return "(T_T)";
        case CHAR_WINK:      return "(-_o)";
        case CHAR_SUSPICIOUS: return "(o_O)";
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
    uint32_t now = millis();
    uint32_t elapsed = now - _stateTime;

    if (_state == CHAR_BEAT || _state == CHAR_DANCE_L || _state == CHAR_DANCE_R || _state == CHAR_JAMMING) {
        if (elapsed > 300) {
            _state = _prevState;
        }
    }
    if (_state == CHAR_SUCCESS || _state == CHAR_ERROR || _state == CHAR_FLIP_BACK
        || _state == CHAR_WINK || _state == CHAR_CRYING || _state == CHAR_DEAD || _state == CHAR_SUSPICIOUS) {
        if (elapsed > 1500) {
            _state = CHAR_IDLE;
        }
    }
    if (_state == CHAR_FLIP) {
        if (elapsed > 1500) {
            _state = CHAR_FLIP_BACK;
            _stateTime = now;
        }
    }
    if (_state == CHAR_STARTLED) {
        if (elapsed > 500) {
            _state = CHAR_IDLE;
        }
    }

    // Sleep after inactivity
    if (_state == CHAR_IDLE && _lastInputTime > 0 && (now - _lastInputTime > SLEEP_TIMEOUT)) {
        _state = CHAR_SLEEPING;
        _stateTime = now;
    }

    if (_message[0] && (now - _msgTime > 1200)) {
        _message[0] = '\0';
    }
}
