#include "platform/display.h"
#include "config.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

static Canvas _canvas;
static SDL_Window* _window = nullptr;
static SDL_Renderer* _renderer = nullptr;
static SDL_Texture* _texture = nullptr;
static SDL_Texture* _skin = nullptr;

// Skin image is 1280x840, we display at half size
static const int SKIN_W = 1280;
static const int SKIN_H = 840;
static const float SKIN_SCALE = 0.5f;
static const int WIN_W = (int)(SKIN_W * SKIN_SCALE);
static const int WIN_H = (int)(SKIN_H * SKIN_SCALE);

// LCD position in skin-pixel space, scaled to window
static const int LCD_X = (int)(323 * SKIN_SCALE);
static const int LCD_Y = (int)(60 * SKIN_SCALE);
static const int LCD_W = (int)(639 * SKIN_SCALE);
static const int LCD_H = (int)(339 * SKIN_SCALE);

void Display::init() {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    _window = SDL_CreateWindow("beepbot dx",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0);
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);

    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);

    // Load device skin
    SDL_Surface* surface = IMG_Load("assets/device_skin.png");
    if (surface) {
        _skin = SDL_CreateTextureFromSurface(_renderer, surface);
        SDL_SetTextureBlendMode(_skin, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(surface);
    }

    _canvas.create(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void Display::beginFrame() {
    _canvas.fillScreen(0x0000);
}

void Display::endFrame() {
    SDL_UpdateTexture(_texture, nullptr, _canvas.buffer(), SCREEN_WIDTH * 2);

    SDL_SetRenderDrawColor(_renderer, 20, 20, 22, 255);
    SDL_RenderClear(_renderer);

    // Blit LCD screen at 2x, centered in window
    int scaledW = SCREEN_WIDTH * 2;
    int scaledH = SCREEN_HEIGHT * 2;
    int offsetX = (WIN_W - scaledW) / 2;
    int offsetY = (WIN_H - scaledH) / 2;
    SDL_Rect lcd = {offsetX, offsetY, scaledW, scaledH};
    SDL_RenderCopy(_renderer, _texture, nullptr, &lcd);

    SDL_RenderPresent(_renderer);
}

Canvas& Display::canvas() {
    return _canvas;
}

void Display::shutdown() {
    if (_skin) SDL_DestroyTexture(_skin);
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    IMG_Quit();
    SDL_Quit();
}
