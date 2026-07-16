#include "app.h"
#include "platform/storage.h"
#include "platform/led.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

App* App::_instance = nullptr;

static const Screen tabMenuScreens[] = { SCREEN_SOUND, SCREEN_PATTERN_SELECT, SCREEN_SONG, SCREEN_PLAY };
static const char* tabMenuLabels[] = { "SOUND", "PATTERN", "SONG", "PLAY" };
static const int tabMenuCount = 4;
static const uint32_t TAB_HOLD_THRESHOLD = 200;

void App::init(AppCallbacks callbacks) {
    _instance = this;
    _callbacks = callbacks;

    Project::init(_project);
    _sequencer.init(&_project);
    _sequencer.setCallback(onTrigger);
    _sequencer.setStepCallback(onStep);
    _character.noteInput();
    GlobalSettings::instance = &_settings;
}

void App::loadSlot(uint8_t slot) {
    _currentProjectSlot = slot;
    Storage::loadProject(_project, slot);
    _project.dirty = false;
    if (_settings.ledMode != LED_OFF) {
        uint8_t r, g, b;
        ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
        LED::setColor(r, g, b);
    } else {
        LED::off();
    }
    getView(_currentScreen)->enter();
}

void App::openProjectList(uint8_t slot) {
    _currentProjectSlot = slot;
    Storage::loadProject(_project, slot);
    _project.dirty = false;
    LED::off();
    _currentScreen = SCREEN_PROJECT;
    _screenBeforeProject = SCREEN_SOUND;
    getView(SCREEN_PROJECT)->enter();
}

void App::onStep(uint8_t step) {
    App* app = _instance;

    // Reset trigger counter for jamming detection
    app->_stepTriggerCount = 0;

    // Pick dance style at start of playback
    if (step == 0 && app->_danceStyle == 0xFF) app->_danceStyle = rand() % 4;

    // Animate on beat
    if (step % 4 == 0) {
        if (app->_danceStyle == 0) {
            app->_character.setState((step / 4) % 2 == 0 ? CHAR_DANCE_R : CHAR_DANCE_L);
        } else if (app->_danceStyle == 1) {
            app->_character.setState((step / 4) % 2 == 0 ? CHAR_HEADPHONES_L : CHAR_HEADPHONES_R);
        } else if (app->_danceStyle == 2) {
            app->_character.setState((step / 4) % 2 == 0 ? CHAR_DANCE_UP_L : CHAR_DANCE_UP_R);
        } else {
            app->_character.setState((step / 4) % 2 == 0 ? CHAR_POINT_R : CHAR_POINT_L);
        }
    }

    // LED
    if (app->_lowBattery) return;
    if (app->_settings.ledMode == LED_OFF) return;
    app->_ledPlaying = true;
    if (step % 4 == 0) {
        uint8_t r, g, b;
        ThemeOps::getPresetRGB(app->_project.themeIndex, r, g, b);
        LED::setColor(r, g, b);
    } else {
        if (app->_settings.ledMode == LED_METRONOME) {
            LED::off();
        }
    }
}

void App::onTrigger(uint8_t soundIndex) {
    App* app = _instance;
    if (soundIndex < NUM_SOUNDS && app->_project.sounds[soundIndex].occupied) {
        SoundSlot& slot = app->_project.sounds[soundIndex];
        Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100, &slot.fx);
    }
    app->_stepTriggerCount++;
    if (app->_stepTriggerCount >= 8) {
        app->_character.setState(CHAR_JAMMING);
    }
    app->_playView.onTrigger(soundIndex);
}

void App::tick() {
    _character.tick();

    if (!_lowBattery && Power::getBatteryPercent() <= 10) {
        _lowBattery = true;
        LED::setColor(255, 0, 0);
    }

    if (Audio::isRecording()) {
        Audio::recordUpdate();
    }

    _sequencer.tick(millis());

    if (_ledPlaying && !_sequencer.isPlaying() && !_lowBattery) {
        _ledPlaying = false;
        _danceStyle = 0xFF;
        if (_settings.ledMode != LED_OFF) {
            uint8_t r, g, b;
            ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
            LED::setColor(r, g, b);
        } else {
            LED::off();
        }
    }

    InputEvent event = Input::poll();

    if (event != INPUT_NONE) {
        if (_character.isSleeping()) {
            _character.wake();
        } else {
            _character.noteInput();
        }
    }

    handleGlobalInput(event);

    View* view = getView(_currentScreen);
    view->update(event);

    handleTransitions();
    render();
}

void App::handleGlobalInput(InputEvent& event) {
    bool textInput = (_currentScreen == SCREEN_SOUND && _soundView.inRename())
                  || (_currentScreen == SCREEN_PROJECT && _projectView.inRename());
    Input::setTextMode(textInput);

    // Help overlay consumes input when open (except screenshot)
    if (_helpOpen && event != INPUT_NONE) {
        if (event == INPUT_CHAR && Input::getChar() == 'y') {
            // fall through to screenshot handler
        } else if (event == INPUT_UP) {
            if (_helpCursor > 0) _helpCursor--;
            event = INPUT_NONE;
            return;
        } else if (event == INPUT_DOWN) {
            if (_helpCursor < _helpTotal - 1) _helpCursor++;
            event = INPUT_NONE;
            return;
        } else {
            _helpOpen = false;
            _helpCursor = 0;
            _helpScroll = 0;
            event = INPUT_NONE;
            return;
        }
    }

    // During text input, view owns all input
    if (textInput) return;

    // Y key takes screenshot
    if (event == INPUT_CHAR && Input::getChar() == 'y') {
        if (_callbacks.onScreenshot && _callbacks.onScreenshot()) {
            _character.setState(CHAR_SUCCESS);
            _character.say("snap!");
        }
        event = INPUT_NONE;
        return;
    }

    // H key opens help
    if (event == INPUT_CHAR && Input::getChar() == 'h'
        && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SETTINGS) {
        _helpOpen = true;
        _helpScroll = 0;
        event = INPUT_NONE;
        return;
    }

    // F key flips table (unless on sound screen where it opens FX)
    if (event == INPUT_CHAR && Input::getChar() == 'f'
        && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SOUND) {
        _character.setState(CHAR_FLIP);
        event = INPUT_NONE;
        return;
    }

    // S key saves project
    if (event == INPUT_CHAR && Input::getChar() == 's'
        && _currentScreen != SCREEN_PROJECT) {
        _character.setState(CHAR_SAVING);
        _character.say("saving...");
        if (Storage::saveProject(_project, _currentProjectSlot)) {
            _project.dirty = false;
            if (_callbacks.saveSlot) _callbacks.saveSlot(_currentProjectSlot);
            _character.setState(CHAR_SUCCESS);
            _character.say("saved!");
        } else {
            _character.setState(CHAR_ERROR);
            _character.say("save failed");
        }
        event = INPUT_NONE;
        return;
    }

    // O key opens project picker
    if (event == INPUT_CHAR && Input::getChar() == 'o'
        && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SETTINGS) {
        _screenBeforeProject = _currentScreen;
        switchScreen(SCREEN_PROJECT);
        event = INPUT_NONE;
        return;
    }

    // G key opens settings
    if (event == INPUT_CHAR && Input::getChar() == 'g'
        && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SETTINGS) {
        _screenBeforeSettings = _currentScreen;
        switchScreen(SCREEN_SETTINGS);
        event = INPUT_NONE;
        return;
    }

    // M key cycles LED mode
    if (event == INPUT_CHAR && Input::getChar() == 'm'
        && _currentScreen != SCREEN_SETTINGS) {
        _settings.ledMode = (LedMode)((_settings.ledMode + 1) % 3);
        const char* names[] = {"led on", "led metro", "led off"};
        _character.say(names[_settings.ledMode]);
        if (_settings.ledMode == LED_OFF) {
            LED::off();
        } else {
            uint8_t r, g, b;
            ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
            LED::setColor(r, g, b);
        }
        if (_callbacks.saveSettings) _callbacks.saveSettings(_settings);
        event = INPUT_NONE;
        return;
    }

    // E key exports song to WAV
    if (event == INPUT_CHAR && Input::getChar() == 'e'
        && (_currentScreen == SCREEN_SONG || _currentScreen == SCREEN_PLAY)) {
        _character.setState(CHAR_SAVING);
        _character.say("rendering...");
        char path[64];
        if (_project.name[0]) {
            snprintf(path, sizeof(path), "/beepbotdx/%s.wav", _project.name);
        } else {
            snprintf(path, sizeof(path), "/beepbotdx/export_%d.wav", _currentProjectSlot + 1);
        }
        if (Storage::renderSongToWav(_project, path)) {
            _character.setState(CHAR_SUCCESS);
            _character.say("exported!");
        } else {
            _character.setState(CHAR_ERROR);
            _character.say("oh no");
        }
        event = INPUT_NONE;
        return;
    }

    // N+plus/minus adjusts brightness
    if ((event == INPUT_PLUS || event == INPUT_MINUS) && Input::isNHeld()) {
        if (event == INPUT_PLUS) {
            _brightness = _brightness <= 90 ? _brightness + 10 : 100;
        } else {
            _brightness = _brightness >= 20 ? _brightness - 10 : 10;
        }
        if (_callbacks.setBrightness) _callbacks.setBrightness(_brightness);
        char msg[12];
        snprintf(msg, sizeof(msg), "bright %d%%", _brightness);
        _character.say(msg);
        event = INPUT_NONE;
        return;
    }

    // L+/- adjusts sample level (in trim or FX)
    if ((event == INPUT_PLUS || event == INPUT_MINUS) && Input::isLHeld()
        && _currentScreen == SCREEN_SOUND && (_soundView.inTrim() || _soundView.inFx())) {
        SoundSlot& slot = _project.sounds[_soundView.getCursor()];
        if (event == INPUT_PLUS) {
            if (slot.level <= 95) slot.level += 5;
            else slot.level = 100;
        } else {
            if (slot.level >= 5) slot.level -= 5;
            else slot.level = 0;
        }
        _project.dirty = true;
        event = INPUT_NONE;
        return;
    }

    // +/- volume or B+/- BPM (not on project screen)
    if ((event == INPUT_PLUS || event == INPUT_MINUS) && _currentScreen != SCREEN_PROJECT) {
        if (Input::isBHeld()) {
            if (event == INPUT_PLUS && _project.bpm < MAX_BPM) { _project.bpm++; _project.dirty = true; }
            if (event == INPUT_MINUS && _project.bpm > MIN_BPM) { _project.bpm--; _project.dirty = true; }
            char bpmMsg[10];
            snprintf(bpmMsg, sizeof(bpmMsg), "bpm %d", _project.bpm);
            _character.say(bpmMsg);
        } else {
            uint8_t vol = Audio::getVolume();
            if (event == INPUT_PLUS) {
                vol = vol <= 245 ? vol + 10 : 255;
            } else {
                vol = vol >= 10 ? vol - 10 : 0;
            }
            Audio::setVolume(vol);
            char volMsg[8];
            snprintf(volMsg, sizeof(volMsg), "vol %d", vol * 100 / 255);
            _character.say(volMsg);
        }
        event = INPUT_NONE;
        return;
    }

    // TAB menu
    if (event == INPUT_TAB && !_tabMenuOpen && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SETTINGS) {
        _tabMenuOpen = true;
        _tabMenuVisible = false;
        _tabMenuMoved = false;
        _tabPressTime = millis();
        for (int i = 0; i < tabMenuCount; i++) {
            if (tabMenuScreens[i] == _currentScreen ||
                (tabMenuScreens[i] == SCREEN_PATTERN_SELECT && _currentScreen == SCREEN_PATTERN_EDIT)) {
                _tabMenuCursor = i;
                break;
            }
        }
        event = INPUT_NONE;
        return;
    }

    if (_tabMenuOpen) {
        if (!Input::isTabHeld()) {
            bool wasVisible = _tabMenuVisible;
            _tabMenuOpen = false;
            _tabMenuVisible = false;
            if (_tabMenuMoved) {
                if (tabMenuScreens[_tabMenuCursor] != _currentScreen) {
                    switchScreen(tabMenuScreens[_tabMenuCursor]);
                }
            } else if (!wasVisible) {
                uint8_t next = (_tabMenuCursor + 1) % tabMenuCount;
                switchScreen(tabMenuScreens[next]);
            }
        } else {
            if (!_tabMenuVisible && (millis() - _tabPressTime >= TAB_HOLD_THRESHOLD)) {
                _tabMenuVisible = true;
            }
            if (event == INPUT_LEFT || event == INPUT_UP) {
                _tabMenuCursor = (_tabMenuCursor + tabMenuCount - 1) % tabMenuCount;
                _tabMenuMoved = true;
            }
            if (event == INPUT_RIGHT || event == INPUT_DOWN) {
                _tabMenuCursor = (_tabMenuCursor + 1) % tabMenuCount;
                _tabMenuMoved = true;
            }
        }
        event = INPUT_NONE;
        return;
    }
}

void App::handleTransitions() {
    if (_currentScreen == SCREEN_PROJECT) {
        if (_projectView.shouldClose()) {
            _projectView.clearClose();
            switchScreen(_screenBeforeProject);
        } else if (_projectView.didLoad()) {
            _projectView.clearLoad();
            _project.dirty = false;
            if (_settings.ledMode != LED_OFF) {
                uint8_t r, g, b;
                ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
                LED::setColor(r, g, b);
            } else {
                LED::off();
            }
            if (_callbacks.saveSlot) _callbacks.saveSlot(_currentProjectSlot);
            switchScreen(SCREEN_SOUND);
        }
    }

    if (_currentScreen == SCREEN_SETTINGS) {
        if (_settingsView.shouldClose()) {
            _settingsView.clearClose();
            if (_callbacks.saveSettings) _callbacks.saveSettings(_settings);
            switchScreen(_screenBeforeSettings);
        }
    }

    if (_currentScreen == SCREEN_PATTERN_SELECT && _patternSelectView.shouldEditPattern()) {
        _patternSelectView.clearEditRequest();
        _patternEditView.setPattern(_patternSelectView.getSelectedPattern());
        _screenBeforePatternEdit = SCREEN_PATTERN_SELECT;
        switchScreen(SCREEN_PATTERN_EDIT);
    }

    if (_currentScreen == SCREEN_PATTERN_EDIT && _patternEditView.shouldGoBack()) {
        _patternEditView.clearBackRequest();
        switchScreen(_screenBeforePatternEdit);
    }

    if (_currentScreen == SCREEN_SONG && _songView.shouldEditPattern()) {
        _songView.clearEditRequest();
        _patternEditView.setPattern(_songView.getEditPattern());
        _screenBeforePatternEdit = SCREEN_SONG;
        switchScreen(SCREEN_PATTERN_EDIT);
    }
}

void App::render() {
    Display::beginFrame();
    Canvas& canvas = Display::canvas();

    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.fillScreen(theme.bg);

    if (_currentScreen == SCREEN_PROJECT && _projectView.inRename()) {
        Theme slotTheme = ThemeOps::getPreset(_projectView.getSlotTheme());
        drawHeader(canvas, slotTheme);
    } else {
        drawHeader(canvas, theme);
    }
    getView(_currentScreen)->draw(canvas);

    if (_helpOpen) drawHelp(canvas, theme);
    if (_tabMenuVisible) drawTabMenu(canvas, theme);

    Display::endFrame();
}

void App::drawHeader(Canvas& canvas, const Theme& theme) {
    const int hdrMargin = 3;
    const int hdrSpacing = 3;
    const int hdrCols = 4;
    const int hdrGridW = SCREEN_WIDTH - hdrMargin * 2;
    const int hdrCellW = (hdrGridW - hdrSpacing * (hdrCols - 1)) / hdrCols;
    const int hdrContentW = hdrCellW * hdrCols + hdrSpacing * (hdrCols - 1);
    const int hdrLeft = hdrMargin + (hdrGridW - hdrContentW) / 2;
    canvas.fillRect(hdrLeft, 3, hdrContentW, 16, theme.accent);

    // Pixelated rounded top corners
    canvas.drawPixel(hdrLeft, 3, theme.bg);
    canvas.drawPixel(hdrLeft + 1, 3, theme.bg);
    canvas.drawPixel(hdrLeft, 4, theme.bg);
    canvas.drawPixel(hdrLeft + hdrContentW - 1, 3, theme.bg);
    canvas.drawPixel(hdrLeft + hdrContentW - 2, 3, theme.bg);
    canvas.drawPixel(hdrLeft + hdrContentW - 1, 4, theme.bg);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);

    // Title (left)
    const char* title = "";
    char titleBuf[16];
    switch (_currentScreen) {
        case SCREEN_SOUND:
            if (_soundView.inProjectInfo()) {
                title = "PROJECT";
            } else if (_soundView.inSubView()) {
                SoundSlot& s = _project.sounds[_soundView.getCursor()];
                if (s.name[0]) {
                    title = s.name;
                } else {
                    snprintf(titleBuf, sizeof(titleBuf), "SND %02d", _soundView.getCursor() + 1);
                    title = titleBuf;
                }
            } else {
                title = "SOUND";
            }
            break;
        case SCREEN_PATTERN_SELECT:
            title = "PATTERN";
            break;
        case SCREEN_PATTERN_EDIT:
            snprintf(titleBuf, sizeof(titleBuf), "PTRN %02d", _patternEditView.getPattern() + 1);
            title = titleBuf;
            break;
        case SCREEN_SONG:
            title = "SONG";
            break;
        case SCREEN_PLAY:
            title = "PLAY";
            break;
        case SCREEN_PROJECT:
            if (_projectView.inRename()) {
                snprintf(titleBuf, sizeof(titleBuf), "PROJ %02d", _projectView.getCursor() + 1);
                title = titleBuf;
            } else {
                title = "PROJECTS";
            }
            break;
        case SCREEN_SETTINGS:
            title = "SETTINGS";
            break;
    }
    canvas.setTextDatum(top_left);
    canvas.drawString(title, hdrLeft + 4, 7);

    // Character face (center)
    const char* face = _character.getFace();
    canvas.setTextDatum(top_center);
    canvas.drawString(face, hdrLeft + hdrContentW / 2, 7);

    // Character message (right of face)
    const char* msg = _character.getMessage();
    if (msg && msg[0]) {
        int faceW = strlen(face) * 6;
        canvas.setTextDatum(top_left);
        canvas.drawString(msg, hdrLeft + hdrContentW / 2 + faceW / 2 + 4, 7);
    }

    // Battery (right)
    char batStr[6];
    snprintf(batStr, sizeof(batStr), "%d%%", Power::getBatteryPercent());
    canvas.setTextDatum(top_right);
    canvas.drawString(batStr, hdrLeft + hdrContentW - 4, 7);
}

struct HelpLine { const char* key; const char* action; };

void App::drawHelp(Canvas& canvas, const Theme& theme) {
    canvas.darken();

    canvas.setTextSize(1);

    static const HelpLine soundHelp[] = {
        {"CTRL/OK", "Select"}, {"G0", "Hold to record"}, {"SPACE", "Audition"},
        {"DEL", "Clear"}, {"I", "Import wav"}, {"R", "Rename"}, {"F", "FX"}, {"T", "Trim"},
    };
    static const HelpLine trimHelp[] = {
        {"L/R", "Adjust point"}, {"U/D", "Switch start/end"}, {"SPACE", "Audition"},
        {"L +/-", "Level"}, {"CTRL/OK", "Apply"}, {"ESC", "Cancel"},
    };
    static const HelpLine fxHelp[] = {
        {"L/R", "Select FX"}, {"U/D", "Adjust value"}, {"CTRL/OK", "Toggle on/off"},
        {"SPACE", "Audition"}, {"L +/-", "Level"}, {"DEL", "Reset"}, {"ESC", "Back"},
    };
    static const HelpLine patSelectHelp[] = {
        {"CTRL/OK", "Edit"}, {"SPACE", "Audition"}, {"DEL", "Clear"},
        {"Fn C", "Copy"}, {"Fn V", "Paste"},
    };
    static const HelpLine patEditHelp[] = {
        {"CTRL/OK", "Toggle step"}, {"SPACE", "Play/stop"}, {"ESC", "Back"},
        {"1-8", "Audition"}, {"Fn 1-8", "Live record"},
    };
    static const HelpLine songHelp[] = {
        {"CTRL/OK", "Edit pattern"}, {"SPACE", "Play song"}, {"DEL", "Clear slot"},
        {"[/]", "Cycle pattern"}, {"E", "Export wav"},
    };
    static const HelpLine playHelp[] = {
        {"SPACE", "Play/stop"}, {"1-8", "Audition"}, {"E", "Export wav"},
    };
    static const HelpLine globals[] = {
        {"S", "Save proj"}, {"O", "Open proj"}, {"G", "Settings"}, {"M", "LED mode"}, {"F", "Table flip"},
        {"+/-", "Volume"}, {"B +/-", "BPM"}, {"N +/-", "Brightness"}, {"TAB", "Navigate"}, {"TAB ^/v ", "Menu"},
    };

    const HelpLine* lines = nullptr;
    int lineCount = 0;
    const char* screenTitle = "";
    bool showGlobals = true;

    switch (_currentScreen) {
        case SCREEN_SOUND:
            if (_soundView.inTrim()) {
                lines = trimHelp; lineCount = 6; screenTitle = "TRIM"; showGlobals = false;
            } else if (_soundView.inFx()) {
                lines = fxHelp; lineCount = 7; screenTitle = "FX"; showGlobals = false;
            } else {
                lines = soundHelp; lineCount = 8; screenTitle = "SOUND";
            }
            break;
        case SCREEN_PATTERN_SELECT:
            lines = patSelectHelp; lineCount = 5; screenTitle = "PATTERN"; break;
        case SCREEN_PATTERN_EDIT:
            lines = patEditHelp; lineCount = 5; screenTitle = "PATTERN EDIT"; break;
        case SCREEN_SONG:
            lines = songHelp; lineCount = 5; screenTitle = "SONG"; break;
        case SCREEN_PLAY:
            lines = playHelp; lineCount = 3; screenTitle = "PLAY"; break;
        default: return;
    }

    // Build flat item list
    const HelpLine* allItems[24];
    int allCount = 0;
    int globalStart = -1;
    for (int i = 0; i < lineCount; i++) allItems[allCount++] = &lines[i];
    if (showGlobals) {
        globalStart = allCount;
        const int globalCount = sizeof(globals) / sizeof(globals[0]);
        for (int i = 0; i < globalCount; i++) allItems[allCount++] = &globals[i];
    }
    _helpTotal = allCount;

    const int startY = 28;
    const int lineHeight = 11;
    const int selectedLineHeight = 11;
    const int groupGap = 8;
    const int labelX = 14;
    const int keyX = SCREEN_WIDTH - 14;

    // Auto-scroll to keep cursor visible
    if (_helpCursor < _helpScroll) _helpScroll = _helpCursor;
    while (_helpScroll < _helpCursor) {
        int y = startY;
        for (int i = _helpScroll; i <= _helpCursor && i < allCount; i++) {
            if (i == globalStart && i > _helpScroll) y += groupGap;
            y += (i == _helpCursor) ? selectedLineHeight : lineHeight;
        }
        if (y <= SCREEN_HEIGHT - 10) break;
        _helpScroll++;
    }

    // Title
    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_left);
    canvas.drawString("HELP", 7, 7);

    // Draw rows
    int y = startY;
    for (int i = _helpScroll; i < allCount; i++) {
        if (i == globalStart && i > _helpScroll) y += groupGap;
        if (y >= SCREEN_HEIGHT - 4) break;

        bool selected = (i == _helpCursor);
        int textY = y;

        canvas.setTextSize(1);
        uint16_t color = selected ? TFT_WHITE : theme.accent;

        canvas.setTextColor(color);
        canvas.setTextDatum(top_left);
        if (selected) canvas.drawString(">", labelX - 8, textY);
        canvas.drawString(allItems[i]->action, labelX, textY);

        canvas.setTextColor(color);
        canvas.setTextDatum(top_left);
        canvas.drawString(allItems[i]->key, 140, textY);

        y += selected ? selectedLineHeight : lineHeight;
    }

    canvas.setTextSize(1);

    if (allCount > 0) {
        const int trackTop = startY;
        const int trackH = SCREEN_HEIGHT - startY - 10;
        int thumbH = trackH / allCount;
        if (thumbH < 6) thumbH = 6;
        int thumbY = trackTop + (trackH - thumbH) * _helpCursor / (allCount - 1);
        canvas.fillRect(SCREEN_WIDTH - 3, thumbY, 3, thumbH, theme.accent);
    }
}

void App::drawTabMenu(Canvas& canvas, const Theme& theme) {
    canvas.darken();

    const int itemH = 16 + 10;
    const int totalH = tabMenuCount * 16 + (tabMenuCount - 1) * 10;
    int startY = (SCREEN_HEIGHT - totalH) / 2;

    canvas.setTextSize(2);
    canvas.setTextDatum(top_left);
    for (int i = 0; i < tabMenuCount; i++) {
        int y = startY + i * itemH;
        if (i == _tabMenuCursor) {
            canvas.setTextColor(TFT_WHITE);
            canvas.drawString(">", 14, y);
            canvas.drawString(tabMenuLabels[i], 30, y);
        } else {
            canvas.setTextColor(theme.accent);
            canvas.drawString(tabMenuLabels[i], 30, y);
        }
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(top_right);
    canvas.setTextColor(theme.accent);
    if (_project.name[0]) {
        canvas.drawString(_project.name, SCREEN_WIDTH - 8, 7);
    } else {
        char slotStr[8];
        snprintf(slotStr, sizeof(slotStr), "PROJ %02d", _currentProjectSlot + 1);
        canvas.drawString(slotStr, SCREEN_WIDTH - 8, 7);
    }
}

View* App::getView(Screen s) {
    switch (s) {
        case SCREEN_SOUND: return &_soundView;
        case SCREEN_PATTERN_SELECT: return &_patternSelectView;
        case SCREEN_PATTERN_EDIT: return &_patternEditView;
        case SCREEN_SONG: return &_songView;
        case SCREEN_PLAY: return &_playView;
        case SCREEN_PROJECT: return &_projectView;
        case SCREEN_SETTINGS: return &_settingsView;
    }
    return &_soundView;
}

void App::switchScreen(Screen s) {
    if (_settings.autoSave && _currentScreen != SCREEN_PROJECT && _currentScreen != SCREEN_SETTINGS) {
        Storage::saveProject(_project, _currentProjectSlot);
        _project.dirty = false;
    }
    getView(_currentScreen)->exit();
    _currentScreen = s;
    getView(_currentScreen)->enter();
}
