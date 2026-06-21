#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/sequencer.h"

class SongView : public View {
public:
    SongView(Project& project, Character& character, Sequencer& sequencer);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    bool shouldEditPattern() const { return _editRequested; }
    void clearEditRequest() { _editRequested = false; }
    uint8_t getEditPattern() const;

private:
    Project& _project;
    Character& _character;
    Sequencer& _sequencer;
    uint8_t _cursor;
    bool _editRequested;
};
