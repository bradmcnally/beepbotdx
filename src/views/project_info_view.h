#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"

class ProjectInfoView : public View {
public:
    ProjectInfoView(Project& project, Character& character);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    bool shouldClose() const { return _closeRequested; }
    void clearClose() { _closeRequested = false; }

private:
    Project& _project;
    Character& _character;
    uint8_t _cursor = 0;
    bool _closeRequested = false;
};
