#include <M5Cardputer.h>
#include "audio.h"
#include "config.h"

static const uint32_t REC_CHUNK = 1024;
static int16_t _recChunk[REC_CHUNK];

static int16_t* _recBuffer = nullptr;
static uint32_t _recMaxLength = 0;
static uint32_t _recOffset = 0;
static bool _recording = false;
static bool _primed = false;

static uint8_t _nextChannel = 0;
static uint8_t _volume = SPEAKER_VOLUME;

void Audio::init() {
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(_volume);
}

void Audio::recordStart(int16_t* buffer, uint32_t maxLength) {
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.end();
    delay(100);

    _recBuffer = buffer;
    _recMaxLength = maxLength;
    _recOffset = 0;
    _recording = true;
    _primed = false;

    memset(_recBuffer, 0, _recMaxLength * sizeof(int16_t));

    auto micCfg = M5Cardputer.Mic.config();
    micCfg.sample_rate = SAMPLE_RATE;
    micCfg.magnification = 64;
    micCfg.noise_filter_level = 0;
    micCfg.over_sampling = 2;
    micCfg.dma_buf_len = 256;
    micCfg.dma_buf_count = 8;
    M5Cardputer.Mic.config(micCfg);
    M5Cardputer.Mic.begin();
    delay(200);

    // Prime the double-buffer — first call queues but data isn't valid yet
    M5Cardputer.Mic.record(_recChunk, REC_CHUNK, SAMPLE_RATE);
}

void Audio::recordUpdate() {
    if (!_recording) return;
    if (_recOffset >= _recMaxLength) return;

    // record() blocks until previous chunk is done, then queues this buffer
    // After returning, _recChunk contains data from the PREVIOUS call
    if (M5Cardputer.Mic.record(_recChunk, REC_CHUNK, SAMPLE_RATE)) {
        if (!_primed) {
            // Skip first return — data isn't valid until second call
            _primed = true;
            return;
        }

        uint32_t remaining = _recMaxLength - _recOffset;
        uint32_t toCopy = (REC_CHUNK < remaining) ? REC_CHUNK : remaining;
        memcpy(_recBuffer + _recOffset, _recChunk, toCopy * sizeof(int16_t));
        _recOffset += toCopy;
    }
}

void Audio::recordStop() {
    _recording = false;
    delay(100);
    M5Cardputer.Mic.end();
    delay(100);

    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(SPEAKER_VOLUME);
    delay(100);
}

bool Audio::isRecording() {
    return _recording;
}

uint32_t Audio::getRecordedLength() {
    return _recOffset;
}

void Audio::triggerSound(const int16_t* buffer, uint32_t length, uint32_t sampleRate, uint8_t volume) {
    if (!buffer || length == 0) return;
    M5Cardputer.Speaker.setChannelVolume(_nextChannel, volume);
    M5Cardputer.Speaker.playRaw(buffer, length, sampleRate, false, 1, _nextChannel);
    _nextChannel = (_nextChannel + 1) % NUM_VOICES;
}

void Audio::stopAll() {
    M5Cardputer.Speaker.stop();
}

void Audio::setVolume(uint8_t vol) {
    _volume = vol;
    M5Cardputer.Speaker.setVolume(_volume);
}

uint8_t Audio::getVolume() {
    return _volume;
}
