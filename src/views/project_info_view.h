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
    bool inRename() const { return _editingName; }
    bool saveRequested() const { return _saveRequested; }
    void clearSaveRequest() { _saveRequested = false; }
    bool exportRequested() const { return _exportRequested; }
    void clearExportRequest() { _exportRequested = false; }
    void showActionResult(bool exporting, bool success);

private:
    Project& _project;
    Character& _character;
    uint8_t _cursor = 0;
    bool _closeRequested = false;
    bool _saveRequested = false;
    bool _exportRequested = false;
    bool _editingName = false;
    char _renameBuffer[9] = {};
    uint8_t _renameLength = 0;
    uint8_t _actionFeedback = 0xFF;
    uint32_t _actionFeedbackTime = 0;
    bool _actionSucceeded = false;
};
