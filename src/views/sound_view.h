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
    bool inFx() const { return _subState == STATE_FX; }
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
        STATE_FX,
        STATE_LOAD_BROWSER,
        STATE_RENAME,
    };

    void updateList(InputEvent event);
    void updateRecordReady(InputEvent event);
    void updateCountdown(InputEvent event);
    void updateRecording(InputEvent event);
    void updateRecordDone();
    void updateTrim(InputEvent event);
    void updateFx(InputEvent event);
    void updateLoadBrowser(InputEvent event);
    void updateRename(InputEvent event);

    void drawList(Canvas& canvas, const struct Theme& theme);
    void drawRecordReady(Canvas& canvas, const struct Theme& theme);
    void drawCountdown(Canvas& canvas, const struct Theme& theme);
    void drawRecording(Canvas& canvas, const struct Theme& theme);
    void drawRecordDone(Canvas& canvas, const struct Theme& theme);
    void drawTrim(Canvas& canvas, const struct Theme& theme);
    void drawFx(Canvas& canvas, const struct Theme& theme);
    void drawKnob(Canvas& canvas, int cx, int cy, int r, uint8_t value,
                  uint8_t maxVal, bool enabled, bool focused, const struct Theme& theme);
    void drawLoadBrowser(Canvas& canvas, const struct Theme& theme);
    void drawRename(Canvas& canvas, const struct Theme& theme);

    void startRecording();
    void stopRecording();
    void triggerSlot(uint8_t index);
    void applyTrim();
    void drawWaveform(Canvas& canvas, const struct Theme& theme, int x, int y, int w, int h);

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

    // FX state
    uint8_t _fxCursor;

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
    uint32_t _recordMaxLength;

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
