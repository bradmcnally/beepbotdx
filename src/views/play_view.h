#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"
#include "core/sequencer.h"
#include "core/bloom_field.h"

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

    BloomField _bloom;
    uint8_t _lastSound;
};
