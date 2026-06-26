#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"

class ProjectView : public View {
public:
    ProjectView(Project& project, Character& character, uint8_t& currentSlot);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    bool shouldClose() const { return _closeRequested; }
    void clearClose() { _closeRequested = false; }
    bool didLoad() const { return _loaded; }
    void clearLoad() { _loaded = false; }

private:
    void doSwitch();

    Project& _project;
    Character& _character;
    uint8_t& _currentSlot;
    uint8_t _cursor;
    bool _closeRequested;
    bool _loaded;
    bool _confirming;
    bool _deleting;
    bool _slotExists[8];
    uint8_t _slotTheme[8];
    uint16_t _slotBpm[8];
    char _statusMsg[24];
    uint32_t _statusTime;
};
