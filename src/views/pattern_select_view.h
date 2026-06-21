#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/sequencer.h"

class PatternSelectView : public View {
public:
    PatternSelectView(Project& project, Character& character, Sequencer& sequencer);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    uint8_t getSelectedPattern() const { return _cursor; }
    bool shouldEditPattern() const { return _editRequested; }
    void clearEditRequest() { _editRequested = false; }

private:
    Project& _project;
    Character& _character;
    Sequencer& _sequencer;
    uint8_t _cursor;
    bool _editRequested;
};
