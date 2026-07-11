#include "platform/display.h"
#include "config.h"
#include <SDL2/SDL.h>

static Canvas _canvas;
static SDL_Window* _window = nullptr;
static SDL_Renderer* _renderer = nullptr;
static SDL_Texture* _texture = nullptr;

static bool _fullscreen = false;
static const int LCD_SCALE = 2;

void Display::init() {
    SDL_Init(SDL_INIT_VIDEO);

    _window = SDL_CreateWindow("beepbot dx",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * LCD_SCALE, SCREEN_HEIGHT * LCD_SCALE,
        SDL_WINDOW_RESIZABLE);
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetIntegerScale(_renderer, SDL_TRUE);

    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);

    _canvas.create(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void Display::toggleFullscreen() {
    _fullscreen = !_fullscreen;
    SDL_SetWindowFullscreen(_window,
        _fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Display::beginFrame() {
    _canvas.fillScreen(0x0000);
}

void Display::endFrame() {
    SDL_UpdateTexture(_texture, nullptr, _canvas.buffer(), SCREEN_WIDTH * 2);

    SDL_SetRenderDrawColor(_renderer, 20, 20, 22, 255);
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
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
