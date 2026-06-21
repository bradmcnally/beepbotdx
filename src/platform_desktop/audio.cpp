#include "platform/audio.h"
#include "config.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdlib>

struct Voice {
    const int16_t* samples;
    uint32_t length;
    uint32_t position;
    uint32_t sampleRate;
    uint8_t volume;
    bool active;
};

static Voice _voices[NUM_VOICES];
static uint8_t _nextChannel = 0;
static uint8_t _volume = SPEAKER_VOLUME;
static SDL_AudioDeviceID _playDev = 0;

static int16_t* _recBuffer = nullptr;
static uint32_t _recMaxLength = 0;
static uint32_t _recOffset = 0;
static bool _recording = false;
static SDL_AudioDeviceID _capDev = 0;

static void audioCallback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    int samples = len / 2;
    memset(out, 0, len);

    for (int i = 0; i < samples; i++) {
        int32_t mix = 0;
        for (int v = 0; v < NUM_VOICES; v++) {
            if (!_voices[v].active) continue;
            int32_t s = _voices[v].samples[_voices[v].position];
            mix += (s * _voices[v].volume) / 255;
            _voices[v].position++;
            if (_voices[v].position >= _voices[v].length) {
                _voices[v].active = false;
            }
        }
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[i] = (int16_t)(mix * _volume / 255);
    }
}

void Audio::init() {
    SDL_Init(SDL_INIT_AUDIO);
    memset(_voices, 0, sizeof(_voices));

    SDL_AudioSpec want = {};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audioCallback;

    _playDev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (_playDev) SDL_PauseAudioDevice(_playDev, 0);
}

void Audio::recordStart(int16_t* buffer, uint32_t maxLength) {
    _recBuffer = buffer;
    _recMaxLength = maxLength;
    _recOffset = 0;
    _recording = true;
    memset(_recBuffer, 0, maxLength * sizeof(int16_t));

    SDL_AudioSpec want = {};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = nullptr;

    _capDev = SDL_OpenAudioDevice(nullptr, 1, &want, nullptr, 0);
    if (_capDev) SDL_PauseAudioDevice(_capDev, 0);
}

void Audio::recordUpdate() {
    if (!_recording || !_capDev) return;
    if (_recOffset >= _recMaxLength) return;

    uint32_t available = SDL_GetQueuedAudioSize(_capDev) / 2;
    uint32_t remaining = _recMaxLength - _recOffset;
    uint32_t toRead = available < remaining ? available : remaining;
    if (toRead > 0) {
        SDL_DequeueAudio(_capDev, _recBuffer + _recOffset, toRead * 2);
        _recOffset += toRead;
    }
}

void Audio::recordStop() {
    _recording = false;
    if (_capDev) {
        SDL_CloseAudioDevice(_capDev);
        _capDev = 0;
    }
}

bool Audio::isRecording() {
    return _recording;
}

uint32_t Audio::getRecordedLength() {
    return _recOffset;
}

void Audio::triggerSound(const int16_t* buffer, uint32_t length, uint32_t sampleRate, uint8_t volume) {
    if (!buffer || length == 0) return;
    SDL_LockAudioDevice(_playDev);
    _voices[_nextChannel].samples = buffer;
    _voices[_nextChannel].length = length;
    _voices[_nextChannel].position = 0;
    _voices[_nextChannel].sampleRate = sampleRate;
    _voices[_nextChannel].volume = volume;
    _voices[_nextChannel].active = true;
    _nextChannel = (_nextChannel + 1) % NUM_VOICES;
    SDL_UnlockAudioDevice(_playDev);
}

void Audio::stopAll() {
    SDL_LockAudioDevice(_playDev);
    for (int i = 0; i < NUM_VOICES; i++) _voices[i].active = false;
    SDL_UnlockAudioDevice(_playDev);
}

void Audio::setVolume(uint8_t vol) {
    _volume = vol;
}

uint8_t Audio::getVolume() {
    return _volume;
}
