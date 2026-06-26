#pragma once

#include "config.h"
#include "core/canvas.h"
#include "core/character.h"
#include "core/project.h"
#include "core/sequencer.h"
#include "core/theme.h"
#include "core/timing.h"
#include "platform/audio.h"
#include "platform/display.h"
#include "platform/input.h"
#include "platform/power.h"
#include "views/view.h"
#include "views/sound_view.h"
#include "views/pattern_select_view.h"
#include "views/pattern_edit_view.h"
#include "views/song_view.h"
#include "views/play_view.h"
#include "views/project_view.h"
#include "views/settings_view.h"

enum Screen {
    SCREEN_SOUND,
    SCREEN_PATTERN_SELECT,
    SCREEN_PATTERN_EDIT,
    SCREEN_SONG,
    SCREEN_PLAY,
    SCREEN_PROJECT,
    SCREEN_SETTINGS,
};

struct AppCallbacks {
    void (*saveSlot)(uint8_t slot);
    void (*setBrightness)(uint8_t percent);
    bool (*onScreenshot)();
    void (*saveSettings)(const GlobalSettings& settings);
};

class App {
public:
    void init(AppCallbacks callbacks);
    void loadSlot(uint8_t slot);
    void tick();

    Project& getProject() { return _project; }
    uint8_t& getCurrentSlot() { return _currentProjectSlot; }
    GlobalSettings& getSettings() { return _settings; }

private:
    void switchScreen(Screen s);
    View* getView(Screen s);
    void handleGlobalInput(InputEvent& event);
    void handleTransitions();
    void render();
    void drawHeader(Canvas& canvas, const Theme& theme);
    void drawHelp(Canvas& canvas, const Theme& theme);
    void drawTabMenu(Canvas& canvas, const Theme& theme);

    static void onTrigger(uint8_t soundIndex);
    static void onStep(uint8_t step);

    bool _lowBattery = false;
    bool _ledPlaying = false;
    uint8_t _stepTriggerCount = 0;

    Project _project;
    Character _character;
    Sequencer _sequencer;
    uint8_t _currentProjectSlot = 0;

    SoundView _soundView{_project, _character};
    PatternSelectView _patternSelectView{_project, _character, _sequencer};
    PatternEditView _patternEditView{_project, _character, _sequencer};
    SongView _songView{_project, _character, _sequencer};
    PlayView _playView{_project, _character, _sequencer};
    ProjectView _projectView{_project, _character, _currentProjectSlot};

    GlobalSettings _settings;
    SettingsView _settingsView{_project, _character, _settings};
    Screen _currentScreen = SCREEN_SOUND;
    Screen _screenBeforeProject = SCREEN_SOUND;
    Screen _screenBeforeSettings = SCREEN_SOUND;
    Screen _screenBeforePatternEdit = SCREEN_PATTERN_SELECT;

    // Help overlay
    bool _helpOpen = false;
    int _helpScroll = 0;

    // Tab menu
    bool _tabMenuOpen = false;
    bool _tabMenuVisible = false;
    bool _tabMenuMoved = false;
    uint32_t _tabPressTime = 0;
    uint8_t _tabMenuCursor = 0;

    // Brightness (managed by platform via callback)
    uint8_t _brightness = 80;

    AppCallbacks _callbacks = {};

    static App* _instance;
};
