#pragma once

enum InputEvent {
    INPUT_NONE,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ENTER,
    INPUT_BACK,
    INPUT_SPACE,
    INPUT_TAB,
    INPUT_NUM1,
    INPUT_NUM2,
    INPUT_NUM3,
    INPUT_NUM4,
    INPUT_NUM5,
    INPUT_NUM6,
    INPUT_NUM7,
    INPUT_NUM8,
    INPUT_NUM9,
    INPUT_NUM0,
    INPUT_PLUS,
    INPUT_MINUS,
    INPUT_CHAR,
    INPUT_ESC,
};

namespace Input {

void init();
InputEvent poll();
char getChar();
bool isBHeld();
bool isFnHeld();
bool isTabHeld();
bool shouldQuit();

}
