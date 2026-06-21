#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/sequencer.h"

class PlayView : public View {
public:
    PlayView(Project& project, Character& character, Sequencer& sequencer);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    void onTrigger(uint8_t soundIndex);

private:
    Project& _project;
    Character& _character;
    Sequencer& _sequencer;

    // Waveform pulse state
    static const int WAVE_W = 220;
    int8_t _wave[WAVE_W];
    uint8_t _pulseEnergy;

    // Floating labels
    struct FloatingLabel {
        char name[9];
        int16_t x;
        int16_t y;
        uint8_t life; // frames remaining
        bool active;
    };
    static const int MAX_FLOATS = 6;
    FloatingLabel _floats[MAX_FLOATS];
    uint8_t _nextFloat;
};
