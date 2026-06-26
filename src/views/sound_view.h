#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/bloom_field.h"

class SoundView : public View {
public:
    SoundView(Project& project, Character& character);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    bool inTrim() const { return _subState == STATE_TRIM; }
    bool inSubView() const { return _subState != STATE_LIST; }
    bool inRename() const { return _subState == STATE_RENAME; }
    uint8_t getCursor() const { return _cursor; }

private:
    enum SubState {
        STATE_LIST,
        STATE_RECORD_READY,
        STATE_COUNTDOWN,
        STATE_RECORDING,
        STATE_RECORD_DONE,
        STATE_TRIM,
        STATE_LOAD_BROWSER,
        STATE_RENAME,
    };

    void startRecording();
    void stopRecording();
    void triggerSlot(uint8_t index);
    void applyTrim();
    void drawWaveform(Canvas& canvas, int x, int y, int w, int h);

    Project& _project;
    Character& _character;
    uint8_t _cursor;
    SubState _subState;
    char _wavFiles[64][32];
    uint8_t _wavFileCount;
    uint8_t _fileCursor;
    char _statusMsg[32];
    uint32_t _statusTime;

    // Trim state
    uint32_t _trimStart;
    uint32_t _trimEnd;
    bool _trimMovingEnd;

    // Rename state
    char _renameBuffer[9];
    uint8_t _renameLen;

    // Preview slot for file browser
    SoundSlot _previewSlot;

    // Countdown state
    uint32_t _countdownStart;
    uint8_t _countdownBeat;

    // Bloom field for recording visualization
    BloomField _bloom;
    uint32_t _recordStartTime;
    uint32_t _recordDoneTime;

    // Audition flash
    uint8_t _flashSlot;
    uint32_t _flashTime;

    // Trim playhead
    uint32_t _playbackStart;
    uint32_t _playbackLength;
    uint32_t _playbackRate;
    bool _playbackActive;

    // Delete confirmation
    bool _confirmingDelete;
};
