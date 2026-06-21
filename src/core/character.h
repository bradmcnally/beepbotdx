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
};

class Character {
public:
    void setState(CharacterState state);
    void say(const char* msg);
    CharacterState getState() const;
    const char* getFace() const;
    const char* getMessage() const;
    bool hasMessage() const;
    void tick();

private:
    CharacterState _state = CHAR_IDLE;
    CharacterState _prevState = CHAR_IDLE;
    uint32_t _stateTime = 0;
    char _message[16];
    uint32_t _msgTime = 0;
};
