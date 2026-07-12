#include "platform/audio.h"
#include "core/slot_fx.h"
#include "core/fx_dsp.h"
#include "config.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdlib>

struct Voice {
    const int16_t* samples;
    uint32_t length;
    uint32_t sampleRate;
    uint8_t volume;
    bool active;
    SlotFx fx;
    FxFilterState filterState;
    double fracPos;
};

static Voice _voices[NUM_VOICES];
static uint8_t _nextChannel = 0;
static uint8_t _volume = SPEAKER_VOLUME;
static SDL_AudioDeviceID _playDev = 0;
static int _playDevRate = SAMPLE_RATE;

static int16_t* _recBuffer = nullptr;
static uint32_t _recMaxLength = 0;
static uint32_t _recOffset = 0;
static bool _recording = false;
static SDL_AudioDeviceID _capDev = 0;
static int _capDevRate = SAMPLE_RATE;

static void audioCallback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    int samples = len / 2;
    memset(out, 0, len);

    for (int i = 0; i < samples; i++) {
        int32_t mix = 0;
        for (int v = 0; v < NUM_VOICES; v++) {
            if (!_voices[v].active) continue;

            double step = (double)_voices[v].sampleRate / _playDevRate;
            if (_voices[v].fx.enabled[FX_PITCH]) {
                step *= FxDsp::pitchRate(_voices[v].fx.value[FX_PITCH]);
            }

            uint32_t idx = (uint32_t)_voices[v].fracPos;
            double frac = _voices[v].fracPos - idx;
            int16_t s;
            if (idx + 1 < _voices[v].length) {
                s = (int16_t)(_voices[v].samples[idx] * (1.0 - frac) +
                              _voices[v].samples[idx + 1] * frac);
            } else {
                s = _voices[v].samples[idx];
            }

            s = FxDsp::processSample(s, _voices[v].fx.value,
                                     _voices[v].fx.enabled, _voices[v].filterState,
                                     (float)_voices[v].sampleRate);

            mix += (s * _voices[v].volume) / 255;
            _voices[v].fracPos += step;
            if ((uint32_t)_voices[v].fracPos >= _voices[v].length) {
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

    SDL_AudioSpec have = {};
    _playDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    _playDevRate = _playDev ? have.freq : SAMPLE_RATE;
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

    SDL_AudioSpec have = {};
    _capDev = SDL_OpenAudioDevice(nullptr, 1, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    _capDevRate = _capDev ? have.freq : SAMPLE_RATE;
    if (_capDev) SDL_PauseAudioDevice(_capDev, 0);
}

void Audio::recordUpdate() {
    if (!_recording || !_capDev) return;
    if (_recOffset >= _recMaxLength) return;

    uint32_t available = SDL_GetQueuedAudioSize(_capDev) / 2;
    if (available == 0) return;

    if (_capDevRate == SAMPLE_RATE) {
        uint32_t remaining = _recMaxLength - _recOffset;
        uint32_t toRead = available < remaining ? available : remaining;
        SDL_DequeueAudio(_capDev, _recBuffer + _recOffset, toRead * 2);
        _recOffset += toRead;
    } else {
        int16_t tmp[4096];
        uint32_t toRead = available < 4096 ? available : 4096;
        SDL_DequeueAudio(_capDev, tmp, toRead * 2);

        double ratio = (double)SAMPLE_RATE / _capDevRate;
        uint32_t outCount = (uint32_t)(toRead * ratio);
        uint32_t remaining = _recMaxLength - _recOffset;
        if (outCount > remaining) outCount = remaining;

        for (uint32_t i = 0; i < outCount; i++) {
            double srcPos = i / ratio;
            uint32_t idx = (uint32_t)srcPos;
            double frac = srcPos - idx;
            if (idx + 1 < toRead)
                _recBuffer[_recOffset + i] = (int16_t)(tmp[idx] * (1.0 - frac) + tmp[idx + 1] * frac);
            else
                _recBuffer[_recOffset + i] = tmp[idx];
        }
        _recOffset += outCount;
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

void Audio::triggerSound(const int16_t* buffer, uint32_t length, uint32_t sampleRate, uint8_t volume, const SlotFx* fx) {
    if (!buffer || length == 0) return;
    SDL_LockAudioDevice(_playDev);
    Voice& v = _voices[_nextChannel];
    v.samples = buffer;
    v.length = length;
    v.sampleRate = sampleRate;
    v.volume = volume;
    v.active = true;
    v.fracPos = 0.0;
    if (fx) {
        v.fx = *fx;
    } else {
        SlotFxOps::defaults(v.fx);
    }
    FxDsp::initFilterState(v.filterState);
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
