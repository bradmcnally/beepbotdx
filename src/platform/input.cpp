#include <M5Cardputer.h>
#include "input.h"

static const char KEY_UP    = ';';
static const char KEY_DOWN  = '.';
static const char KEY_LEFT  = ',';
static const char KEY_RIGHT = '/';

static char lastChar = 0;

static unsigned long lastKeyTime = 0;
static unsigned long keyRepeatDelay = 300;
static unsigned long keyRepeatRate = 80;
static bool keyRepeating = false;
static char lastArrowKey = 0;

static bool isArrowKey(char key) {
    return key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT;
}

static InputEvent arrowToEvent(char key) {
    switch (key) {
        case KEY_UP:    return INPUT_UP;
        case KEY_DOWN:  return INPUT_DOWN;
        case KEY_LEFT:  return INPUT_LEFT;
        case KEY_RIGHT: return INPUT_RIGHT;
        default:        return INPUT_NONE;
    }
}

void Input::init() {
}

char Input::getChar() {
    return lastChar;
}

static bool bHeld = false;
static bool fnHeld = false;
static bool tabHeld = false;
static bool tabEventSent = false;

bool Input::isBHeld() {
    return bHeld;
}

bool Input::isFnHeld() {
    return fnHeld;
}

bool Input::isTabHeld() {
    return tabHeld;
}

InputEvent Input::poll() {
    auto status = M5Cardputer.Keyboard.keysState();

    // Detect modifiers
    fnHeld = status.fn;
    tabHeld = status.tab;
    bHeld = false;
    for (auto key : status.word) {
        if (key == 'b') { bHeld = true; break; }
    }

    // Track tab press/release for edge detection
    if (!tabHeld) {
        tabEventSent = false;
    }

    // Check for first press (edge-triggered)
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        if (status.enter) { lastArrowKey = 0; return INPUT_ENTER; }
        if (status.del)   { lastArrowKey = 0; return INPUT_BACK; }
        if (status.tab && !tabEventSent) { lastArrowKey = 0; tabEventSent = true; return INPUT_TAB; }

        for (auto key : status.word) {
            if (isArrowKey(key)) {
                lastArrowKey = key;
                lastKeyTime = millis();
                keyRepeating = false;
                return arrowToEvent(key);
            }

            if (key == 'b' && bHeld) continue;

            lastArrowKey = 0;
            switch (key) {
                case ' ':  return INPUT_SPACE;
                case '`':  return INPUT_ESC;
                case '1':  return INPUT_NUM1;
                case '2':  return INPUT_NUM2;
                case '3':  return INPUT_NUM3;
                case '4':  return INPUT_NUM4;
                case '5':  return INPUT_NUM5;
                case '6':  return INPUT_NUM6;
                case '7':  return INPUT_NUM7;
                case '8':  return INPUT_NUM8;
                case '9':  return INPUT_NUM9;
                case '0':  return INPUT_NUM0;
                case '=':  return INPUT_PLUS;
                case '-':  return INPUT_MINUS;
                default:
                    if (key >= 'a' && key <= 'z') {
                        lastChar = key;
                        return INPUT_CHAR;
                    }
                    break;
            }
        }
        return INPUT_NONE;
    }

    // Key repeat for arrow keys while held
    if (lastArrowKey && M5Cardputer.Keyboard.isPressed()) {
        bool stillHeld = false;
        for (auto key : status.word) {
            if (key == lastArrowKey) {
                stillHeld = true;
                break;
            }
        }

        if (stillHeld) {
            unsigned long now = millis();
            unsigned long elapsed = now - lastKeyTime;
            unsigned long threshold = keyRepeating ? keyRepeatRate : keyRepeatDelay;

            if (elapsed >= threshold) {
                keyRepeating = true;
                lastKeyTime = now;
                return arrowToEvent(lastArrowKey);
            }
        } else {
            lastArrowKey = 0;
            keyRepeating = false;
        }
    } else {
        lastArrowKey = 0;
        keyRepeating = false;
    }

    return INPUT_NONE;
}
