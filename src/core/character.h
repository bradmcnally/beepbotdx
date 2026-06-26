#pragma once

#include <cstdint>

enum CharacterState {
    CHAR_IDLE,
    CHAR_FOCUSED,
    CHAR_RECORDING,
    CHAR_PLAYING,
    CHAR_SAVING,
    CHAR_ERROR,
    CHAR_SUCCESS,
    CHAR_BEAT,
    CHAR_FLIP,
    CHAR_FLIP_BACK,
    CHAR_SLEEPING,
    CHAR_STARTLED,
    CHAR_DANCE_L,
    CHAR_DANCE_R,
    CHAR_JAMMING,
};

class Character {
public:
    void setState(CharacterState state);
    void say(const char* msg);
    void noteInput();
    void wake();
    CharacterState getState() const;
    const char* getFace() const;
    const char* getMessage() const;
    bool hasMessage() const;
    bool isSleeping() const { return _state == CHAR_SLEEPING; }
    void tick();

private:
    CharacterState _state = CHAR_IDLE;
    CharacterState _prevState = CHAR_IDLE;
    uint32_t _stateTime = 0;
    uint32_t _lastInputTime = 0;
    char _message[16];
    uint32_t _msgTime = 0;
};
