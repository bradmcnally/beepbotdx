#include "platform/input.h"
#include <SDL2/SDL.h>

static char lastChar = 0;
static bool bHeld = false;
static bool fnHeld = false;
static bool tabHeld = false;

static bool quitRequested = false;
static SDL_Scancode lastArrowKey = SDL_SCANCODE_UNKNOWN;
static uint32_t lastKeyTime = 0;
static bool keyRepeating = false;
static const uint32_t REPEAT_DELAY = 300;
static const uint32_t REPEAT_RATE = 80;

static InputEvent arrowToEvent(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_UP:    return INPUT_UP;
        case SDL_SCANCODE_DOWN:  return INPUT_DOWN;
        case SDL_SCANCODE_LEFT:  return INPUT_LEFT;
        case SDL_SCANCODE_RIGHT: return INPUT_RIGHT;
        default: return INPUT_NONE;
    }
}

static bool isArrow(SDL_Scancode sc) {
    return sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_DOWN ||
           sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT;
}

void Input::init() {}

char Input::getChar() {
    return lastChar;
}

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
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    bHeld = keys[SDL_SCANCODE_B] != 0;
    fnHeld = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    tabHeld = keys[SDL_SCANCODE_TAB] != 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            quitRequested = true;
            return INPUT_NONE;
        }
        if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
            SDL_Scancode sc = ev.key.keysym.scancode;

            if (isArrow(sc)) {
                lastArrowKey = sc;
                lastKeyTime = SDL_GetTicks();
                keyRepeating = false;
                return arrowToEvent(sc);
            }

            lastArrowKey = SDL_SCANCODE_UNKNOWN;
            switch (ev.key.keysym.sym) {
                case SDLK_RETURN: return INPUT_ENTER;
                case SDLK_BACKSPACE: return INPUT_BACK;
                case SDLK_TAB:    return INPUT_TAB;
                case SDLK_SPACE:  return INPUT_SPACE;
                case SDLK_ESCAPE: return INPUT_ESC;
                case SDLK_EQUALS: return INPUT_PLUS;
                case SDLK_MINUS:  return INPUT_MINUS;
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

bool Input::shouldQuit() {
    return quitRequested;
}
