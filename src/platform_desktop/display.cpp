#include "platform/display.h"
#include "config.h"
#include <SDL2/SDL.h>

static Canvas _canvas;
static SDL_Window* _window = nullptr;
static SDL_Renderer* _renderer = nullptr;
static SDL_Texture* _texture = nullptr;

static int _winW;
static int _winH;

static const int LCD_SCALE = 2;

void Display::init() {
    SDL_Init(SDL_INIT_VIDEO);
    _winW = SCREEN_WIDTH * LCD_SCALE;
    _winH = SCREEN_HEIGHT * LCD_SCALE;

    _window = SDL_CreateWindow("beepbot dx",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        _winW, _winH, 0);
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);

    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);

    _canvas.create(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void Display::beginFrame() {
    _canvas.fillScreen(0x0000);
}

void Display::endFrame() {
    SDL_UpdateTexture(_texture, nullptr, _canvas.buffer(), SCREEN_WIDTH * 2);

    SDL_SetRenderDrawColor(_renderer, 20, 20, 22, 255);
    SDL_RenderClear(_renderer);

    SDL_Rect lcd = {0, 0, _winW, _winH};
    SDL_RenderCopy(_renderer, _texture, nullptr, &lcd);

    SDL_RenderPresent(_renderer);
}

Canvas& Display::canvas() {
    return _canvas;
}

void Display::shutdown() {
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Display::setBrightness(uint8_t value) {
    (void)value;
}
