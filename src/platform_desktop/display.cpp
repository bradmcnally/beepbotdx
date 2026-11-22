#include "platform/display.h"
#include "config.h"
#include <SDL2/SDL.h>
#ifdef CARDPUTER_ZERO_DEVICE
#include "native_io.h"
#endif

static Canvas _canvas;
#ifndef CARDPUTER_ZERO_DEVICE
static SDL_Window* _window = nullptr;
static SDL_Renderer* _renderer = nullptr;
static SDL_Texture* _texture = nullptr;

static bool _fullscreen = false;
static const int LCD_SCALE = 2;
#endif

void Display::init() {
#ifndef CARDPUTER_ZERO_DEVICE
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
#endif

    _canvas.create(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void Display::toggleFullscreen() {
#ifndef CARDPUTER_ZERO_DEVICE
    _fullscreen = !_fullscreen;
    SDL_SetWindowFullscreen(_window,
        _fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#endif
}

void Display::beginFrame() {
    _canvas.fillScreen(0x0000);
}

void Display::endFrame() {
#ifdef CARDPUTER_ZERO_DEVICE
    Zero::nativeIo.present(_canvas.buffer());
#else
    SDL_UpdateTexture(_texture, nullptr, _canvas.buffer(), SCREEN_WIDTH * 2);

    SDL_SetRenderDrawColor(_renderer, 20, 20, 22, 255);
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
#endif
}

Canvas& Display::canvas() {
    return _canvas;
}

void Display::shutdown() {
#ifndef CARDPUTER_ZERO_DEVICE
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
#endif
    SDL_Quit();
}

void Display::setBrightness(uint8_t value) {
    (void)value;
}
