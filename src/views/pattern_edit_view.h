#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/sequencer.h"

class PatternEditView : public View {
public:
    PatternEditView(Project& project, Character& character, Sequencer& sequencer);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    void setPattern(uint8_t index) { _patternIndex = index; }
    uint8_t getPattern() const { return _patternIndex; }
    bool shouldGoBack() const { return _backRequested; }
    void clearBackRequest() { _backRequested = false; }

private:
    void toggleStep(uint8_t sound);
    void toggleStepAt(uint8_t step);
    void triggerSound(uint8_t sound);
    void handleNumber(uint8_t num);

    Project& _project;
    Character& _character;
    Sequencer& _sequencer;
    uint8_t _patternIndex;
    uint8_t _cursorX; // step 0-15
    uint8_t _cursorY; // sound 0-7
    bool _backRequested;
    uint8_t _flashSound;
    uint32_t _flashTime;
};
