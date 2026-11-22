#include "platform/input.h"
#include "platform/display.h"
#include <SDL2/SDL.h>
#ifdef CARDPUTER_ZERO_DEVICE
#include "native_io.h"
#endif

static char lastChar = 0;
static bool textMode = false;
static bool bHeld = false;
static bool lHeld = false;
static bool nHeld = false;
static bool fnHeld = false;
static bool tabHeld = false;

static bool quitRequested = false;
static SDL_Scancode lastArrowKey = SDL_SCANCODE_UNKNOWN;
static uint32_t lastKeyTime = 0;
static bool keyRepeating = false;
static const uint32_t REPEAT_DELAY = 300;
static const uint32_t REPEAT_RATE = 80;
#ifdef CARDPUTER_ZERO
static bool recordHeld = false;
static bool recordPressed = false;
static bool recordReleased = false;
#endif
#ifdef CARDPUTER_ZERO_DEVICE
static int brightness = 0;
#endif

static InputEvent arrowToEvent(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_UP:    return INPUT_UP;
        case SDL_SCANCODE_DOWN:  return INPUT_DOWN;
        case SDL_SCANCODE_LEFT:  return INPUT_LEFT;
        case SDL_SCANCODE_RIGHT: return INPUT_RIGHT;
#ifdef CARDPUTER_ZERO
        case SDL_SCANCODE_F: return textMode ? INPUT_NONE : INPUT_UP;
        case SDL_SCANCODE_X: return textMode ? INPUT_NONE : INPUT_DOWN;
        case SDL_SCANCODE_Z: return textMode ? INPUT_NONE : INPUT_LEFT;
        case SDL_SCANCODE_C: return textMode ? INPUT_NONE : INPUT_RIGHT;
#endif
        default: return INPUT_NONE;
    }
}

void Input::init() {}

char Input::getChar() {
    return lastChar;
}

bool Input::isBHeld() {
    return bHeld;
}

bool Input::isLHeld() {
    return lHeld;
}

bool Input::isNHeld() {
    return nHeld;
}

bool Input::isFnHeld() {
    return fnHeld;
}

bool Input::isTabHeld() {
    return tabHeld;
}

static const Uint8* keyboardState() {
#ifdef CARDPUTER_ZERO_DEVICE
    return Zero::nativeIo.keyboardState();
#else
    return SDL_GetKeyboardState(nullptr);
#endif
}

static void updateHeldKeys(const Uint8* keys) {
    bHeld = keys[SDL_SCANCODE_B] != 0;
    lHeld = keys[SDL_SCANCODE_L] != 0;
    nHeld = keys[SDL_SCANCODE_N] != 0;
    fnHeld = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    tabHeld = keys[SDL_SCANCODE_TAB] != 0;
}

InputEvent Input::poll() {
    const Uint8* keys = keyboardState();
    updateHeldKeys(keys);
#ifdef CARDPUTER_ZERO
    recordPressed = recordReleased = false;
#endif

    SDL_Event ev;
#ifdef CARDPUTER_ZERO_DEVICE
    while (Zero::nativeIo.pollEvent(&ev)) {
        updateHeldKeys(keys);
        if (Zero::nativeIo.isHomeEvent(ev)) return INPUT_HOME;
        if (Zero::nativeIo.isBacklightEvent(ev)) {
            brightness = ev.user.code;
            return INPUT_BRIGHTNESS;
        }
#else
    while (SDL_PollEvent(&ev)) {
#endif
        if (ev.type == SDL_QUIT) {
            quitRequested = true;
            return INPUT_NONE;
        }
#ifdef CARDPUTER_ZERO
        if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
            const SDL_Scancode sc = ev.key.keysym.scancode;
            if (sc == SDL_SCANCODE_INSERT && ev.key.repeat == 0) {
                const bool down = ev.type == SDL_KEYDOWN;
                recordPressed = down && !recordHeld;
                recordReleased = !down && recordHeld;
                recordHeld = down;
                return INPUT_NONE;
            }
            if (ev.type == SDL_KEYDOWN && sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F8) {
                if (ev.key.repeat != 0) continue;
                fnHeld = true;
                return static_cast<InputEvent>(INPUT_NUM1 + sc - SDL_SCANCODE_F1);
            }
            if (ev.type == SDL_KEYDOWN && (ev.key.keysym.mod & KMOD_CTRL) &&
                (sc == SDL_SCANCODE_C || sc == SDL_SCANCODE_V)) {
                if (ev.key.repeat != 0) continue;
                fnHeld = true;
                lastChar = sc == SDL_SCANCODE_C ? 'c' : 'v';
                return INPUT_CHAR;
            }
        }
#endif
        if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
            SDL_Scancode sc = ev.key.keysym.scancode;

#ifdef CARDPUTER_ZERO
            if ((ev.key.keysym.mod & KMOD_CTRL) &&
                (sc == SDL_SCANCODE_J || sc == SDL_SCANCODE_K)) {
                return sc == SDL_SCANCODE_J ? INPUT_LEVEL_DOWN : INPUT_LEVEL_UP;
            }
#endif

#ifdef CARDPUTER_ZERO_DEVICE
            if (ev.key.keysym.mod & KMOD_MODE) {
                if (textMode && ev.key.keysym.sym >= ' ' && ev.key.keysym.sym <= '~') {
                    lastChar = static_cast<char>(ev.key.keysym.sym);
                    return INPUT_CHAR;
                }
                continue;
            }
#endif

            if (sc == SDL_SCANCODE_F11) {
                Display::toggleFullscreen();
                return INPUT_NONE;
            }

            const InputEvent direction = arrowToEvent(sc);
            if (direction != INPUT_NONE) {
                lastArrowKey = sc;
                lastKeyTime = SDL_GetTicks();
                keyRepeating = false;
                return direction;
            }

#ifndef CARDPUTER_ZERO
            lastArrowKey = SDL_SCANCODE_UNKNOWN;
#endif
            switch (ev.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
#ifndef CARDPUTER_ZERO
                case SDLK_RCTRL:
                case SDLK_LCTRL:
#endif
                    return INPUT_ENTER;
                case SDLK_BACKSPACE:
                case SDLK_DELETE: return INPUT_BACK;
                case SDLK_TAB:    return INPUT_TAB;
                case SDLK_SPACE:  return INPUT_SPACE;
                case SDLK_ESCAPE: return INPUT_ESC;
                case SDLK_EQUALS:
                case SDLK_KP_PLUS:
                case SDLK_VOLUMEUP: return INPUT_PLUS;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                case SDLK_VOLUMEDOWN: return INPUT_MINUS;
                case SDLK_1:      return INPUT_NUM1;
                case SDLK_2:      return INPUT_NUM2;
                case SDLK_3:      return INPUT_NUM3;
                case SDLK_4:      return INPUT_NUM4;
                case SDLK_5:      return INPUT_NUM5;
                case SDLK_6:      return INPUT_NUM6;
                case SDLK_7:      return INPUT_NUM7;
                case SDLK_8:      return INPUT_NUM8;
                case SDLK_9:      return INPUT_NUM9;
                case SDLK_0:      return INPUT_NUM0;
                case SDLK_LEFTBRACKET:
#ifdef CARDPUTER_ZERO
                case SDLK_HOME:
#endif
                    lastChar = '[';
                    return INPUT_CHAR;
                case SDLK_RIGHTBRACKET:
#ifdef CARDPUTER_ZERO
                case SDLK_END:
#endif
                    lastChar = ']';
                    return INPUT_CHAR;
#ifdef CARDPUTER_ZERO
                case SDLK_HELP:
                    lastChar = 'h';
                    return INPUT_CHAR;
#endif
                default:
                    if (ev.key.keysym.sym >= SDLK_a && ev.key.keysym.sym <= SDLK_z) {
                        lastChar = (char)ev.key.keysym.sym;
                        return INPUT_CHAR;
                    }
                    break;
            }
        }
        if (ev.type == SDL_KEYUP) {
            if (ev.key.keysym.scancode == lastArrowKey) {
                lastArrowKey = SDL_SCANCODE_UNKNOWN;
                keyRepeating = false;
            }
        }
    }

    // Key repeat for held arrow keys
    if (lastArrowKey != SDL_SCANCODE_UNKNOWN && keys[lastArrowKey]) {
        uint32_t now = SDL_GetTicks();
        uint32_t threshold = keyRepeating ? REPEAT_RATE : REPEAT_DELAY;
        if (now - lastKeyTime >= threshold) {
            keyRepeating = true;
            lastKeyTime = now;
            return arrowToEvent(lastArrowKey);
        }
    }

    return INPUT_NONE;
}

void Input::setTextMode(bool enabled) {
    textMode = enabled;
}

bool Input::isTextMode() {
    return textMode;
}

bool Input::isRecordPressed() {
#ifdef CARDPUTER_ZERO
    return recordHeld;
#else
    return false;
#endif
}

bool Input::wasRecordPressed() {
#ifdef CARDPUTER_ZERO
    return recordPressed;
#else
    return false;
#endif
}

bool Input::wasRecordReleased() {
#ifdef CARDPUTER_ZERO
    return recordReleased;
#else
    return false;
#endif
}

bool Input::shouldQuit() {
    return quitRequested;
}

#ifdef CARDPUTER_ZERO_DEVICE
int Input::brightnessPercent() {
    return brightness;
}

int Input::homeSecondsRemaining() {
    return Zero::nativeIo.homeSecondsRemaining();
}
#endif
