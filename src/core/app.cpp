#include "app.h"
#include "platform/storage.h"
#include "platform/led.h"
#include <cstdio>
#include <cstring>

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
}

void App::loadSlot(uint8_t slot) {
    _currentProjectSlot = slot;
    Storage::loadProject(_project, slot);
    uint8_t r, g, b;
    ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
    LED::setColor(r, g, b);
    getView(_currentScreen)->enter();
}

void App::onStep(uint8_t step) {
    App* app = _instance;
    if (app->_lowBattery) return;
    if (app->_settings.ledMode == LED_OFF) return;
    app->_ledPlaying = true;
    if (step % 4 == 0) {
        uint8_t r, g, b;
        ThemeOps::getPresetRGB(app->_project.themeIndex, r, g, b);
        LED::setColor(r, g, b);
    } else {
        if (app->_settings.ledMode == LED_BEAT) {
            LED::off();
        }
    }
}

void App::onTrigger(uint8_t soundIndex) {
    App* app = _instance;
    if (soundIndex < NUM_SOUNDS && app->_project.sounds[soundIndex].occupied) {
        SoundSlot& slot = app->_project.sounds[soundIndex];
        Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100);
    }
    app->_character.setState(CHAR_BEAT);
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
        uint8_t r, g, b;
        ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
        LED::setColor(r, g, b);
    }

    InputEvent event = Input::poll();

    handleGlobalInput(event);

    View* view = getView(_currentScreen);
    view->update(event);

    handleTransitions();
    render();
}

void App::handleGlobalInput(InputEvent& event) {
    bool textInput = (_currentScreen == SCREEN_SOUND && _soundView.inRename());

    // Help overlay consumes input when open
    if (_helpOpen && event != INPUT_NONE) {
        if (event == INPUT_UP) {
            if (_helpScroll > 0) _helpScroll--;
        } else if (event == INPUT_DOWN) {
            _helpScroll++;
        } else {
            _helpOpen = false;
            _helpScroll = 0;
        }
        event = INPUT_NONE;
        return;
    }

    // Y key takes screenshot
    if (event == INPUT_CHAR && Input::getChar() == 'y'
        && !textInput) {
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
        && !textInput) {
        _helpOpen = true;
        _helpScroll = 0;
        event = INPUT_NONE;
        return;
    }

    // F key flips table
    if (event == INPUT_CHAR && Input::getChar() == 'f'
        && _currentScreen != SCREEN_PROJECT
        && !textInput) {
        _character.setState(CHAR_FLIP);
        event = INPUT_NONE;
        return;
    }

    // S key saves project
    if (event == INPUT_CHAR && Input::getChar() == 's'
        && _currentScreen != SCREEN_PROJECT
        && !textInput) {
        _character.setState(CHAR_SAVING);
        _character.say("saving...");
        if (Storage::saveProject(_project, _currentProjectSlot)) {
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
        && _currentScreen != SCREEN_SETTINGS
        && !textInput) {
        _screenBeforeProject = _currentScreen;
        switchScreen(SCREEN_PROJECT);
        event = INPUT_NONE;
        return;
    }

    // G key opens settings
    if (event == INPUT_CHAR && Input::getChar() == 'g'
        && _currentScreen != SCREEN_PROJECT
        && _currentScreen != SCREEN_SETTINGS
        && !textInput) {
        _screenBeforeSettings = _currentScreen;
        switchScreen(SCREEN_SETTINGS);
        event = INPUT_NONE;
        return;
    }

    // M key toggles LED metronome
    if (event == INPUT_CHAR && Input::getChar() == 'm'
        && _currentScreen != SCREEN_SETTINGS
        && !textInput) {
        if (_settings.ledMode == LED_OFF) {
            _settings.ledMode = LED_ON;
            _character.say("led on");
        } else {
            _settings.ledMode = LED_OFF;
            _character.say("led off");
        }
        event = INPUT_NONE;
        return;
    }

    // Fn+plus/minus adjusts brightness
    if ((event == INPUT_PLUS || event == INPUT_MINUS) && Input::isFnHeld()
        && !textInput) {
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

    // +/- volume or B+/- BPM (not on project screen, not in trim/rename)
    if ((event == INPUT_PLUS || event == INPUT_MINUS) && _currentScreen != SCREEN_PROJECT
        && !(_currentScreen == SCREEN_SOUND && _soundView.inTrim())
        && !textInput) {
        if (Input::isBHeld()) {
            if (event == INPUT_PLUS && _project.bpm < MAX_BPM) _project.bpm++;
            if (event == INPUT_MINUS && _project.bpm > MIN_BPM) _project.bpm--;
            if (_currentScreen == SCREEN_PLAY || _currentScreen == SCREEN_SONG || _currentScreen == SCREEN_PATTERN_SELECT) {
                char bpmMsg[10];
                snprintf(bpmMsg, sizeof(bpmMsg), "bpm %d", _project.bpm);
                _character.say(bpmMsg);
            }
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
        && _currentScreen != SCREEN_SETTINGS && !textInput) {
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
            if (_callbacks.saveSlot) _callbacks.saveSlot(_currentProjectSlot);
            switchScreen(SCREEN_SOUND);
        }
    }

    if (_currentScreen == SCREEN_SETTINGS) {
        if (_settingsView.shouldClose()) {
            _settingsView.clearClose();
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

    if (_currentScreen != SCREEN_PROJECT) {
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
            if (_soundView.inSubView()) {
                snprintf(titleBuf, sizeof(titleBuf), "SND %02d", _soundView.getCursor() + 1);
                title = titleBuf;
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
            title = "PROJECTS";
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
    uint16_t* buf = canvas.buffer();
    int total = SCREEN_WIDTH * SCREEN_HEIGHT;
    for (int p = 0; p < total; p++) {
        uint16_t c = buf[p];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        buf[p] = ((r * 15 / 100) << 11) | ((g * 15 / 100) << 5) | (b * 15 / 100);
    }

    canvas.setTextSize(1);

    static const HelpLine soundHelp[] = {
        {"ENTER", "open slot"}, {"SPACE", "audition"}, {"DEL", "clear"},
        {"I", "import wav"}, {"R", "rename"}, {"1-8", "audition"},
    };
    static const HelpLine trimHelp[] = {
        {"L/R", "adjust point"}, {"U/D", "switch start/end"}, {"SPACE", "audition"},
        {"+/-", "volume"}, {"ENTER", "apply"}, {"ESC", "cancel"},
    };
    static const HelpLine patSelectHelp[] = {
        {"ENTER", "edit"}, {"SPACE", "audition"}, {"DEL", "clear"},
        {"Fn+C", "copy"}, {"Fn+V", "paste"},
    };
    static const HelpLine patEditHelp[] = {
        {"ENTER", "toggle step"}, {"SPACE", "play/stop"}, {"ESC", "back"},
        {"1-8", "audition"},
    };
    static const HelpLine songHelp[] = {
        {"ENTER", "edit pattern"}, {"SPACE", "play song"}, {"DEL", "clear slot"},
        {"N/P", "cycle pattern"},
    };
    static const HelpLine playHelp[] = {
        {"SPACE", "play/stop"}, {"1-8", "audition"},
    };
    static const HelpLine globals[] = {
        {"S", "save"}, {"O", "open"}, {"G", "settings"}, {"M", "led"},
        {"+/-", "volume"}, {"B+/-", "bpm"}, {"Fn+/-", "bright"}, {"TAB", "navigate"},
    };

    const HelpLine* lines = nullptr;
    int lineCount = 0;
    const char* screenTitle = "";
    bool showGlobals = true;

    switch (_currentScreen) {
        case SCREEN_SOUND:
            if (_soundView.inTrim()) {
                lines = trimHelp; lineCount = 6; screenTitle = "TRIM"; showGlobals = false;
            } else {
                lines = soundHelp; lineCount = 6; screenTitle = "SOUND";
            }
            break;
        case SCREEN_PATTERN_SELECT:
            lines = patSelectHelp; lineCount = 5; screenTitle = "PATTERN"; break;
        case SCREEN_PATTERN_EDIT:
            lines = patEditHelp; lineCount = 4; screenTitle = "PATTERN EDIT"; break;
        case SCREEN_SONG:
            lines = songHelp; lineCount = 4; screenTitle = "SONG"; break;
        case SCREEN_PLAY:
            lines = playHelp; lineCount = 2; screenTitle = "PLAY"; break;
        default: return;
    }

    int totalLines = lineCount + (showGlobals ? 1 + 6 : 0);
    const int visibleLines = 12;
    int maxScroll = totalLines > visibleLines ? totalLines - visibleLines : 0;
    if (_helpScroll > maxScroll) _helpScroll = maxScroll;

    int y = 10;

    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_center);
    canvas.drawString(screenTitle, SCREEN_WIDTH / 2, y);
    y += 12;

    int lineIdx = 0;

    canvas.setTextDatum(top_left);
    for (int i = 0; i < lineCount; i++) {
        if (lineIdx >= _helpScroll && lineIdx < _helpScroll + visibleLines) {
            canvas.setTextColor(theme.accent);
            canvas.drawString(lines[i].key, 14, y);
            canvas.setTextColor(TFT_WHITE);
            canvas.drawString(lines[i].action, 60, y);
            y += 9;
        }
        lineIdx++;
    }

    if (showGlobals) {
        if (lineIdx >= _helpScroll && lineIdx < _helpScroll + visibleLines) {
            y += 3;
            canvas.setTextColor(theme.dim);
            canvas.setTextDatum(top_center);
            canvas.drawString("-- global --", SCREEN_WIDTH / 2, y);
            y += 10;
        }
        lineIdx++;
        canvas.setTextDatum(top_left);
        for (int i = 0; i < 6; i++) {
            if (lineIdx >= _helpScroll && lineIdx < _helpScroll + visibleLines) {
                canvas.setTextColor(theme.accent);
                canvas.drawString(globals[i].key, 14, y);
                canvas.setTextColor(TFT_WHITE);
                canvas.drawString(globals[i].action, 60, y);
                y += 9;
            }
            lineIdx++;
        }
    }

    if (maxScroll > 0) {
        const int trackTop = 10;
        const int trackH = SCREEN_HEIGHT - 20;
        int thumbH = trackH * visibleLines / totalLines;
        if (thumbH < 6) thumbH = 6;
        int thumbY = trackTop + (trackH - thumbH) * _helpScroll / maxScroll;
        canvas.fillRect(SCREEN_WIDTH - 3, thumbY, 3, thumbH, theme.accent);
    }
}

void App::drawTabMenu(Canvas& canvas, const Theme& theme) {
    uint16_t* buf = canvas.buffer();
    int total = SCREEN_WIDTH * SCREEN_HEIGHT;
    for (int p = 0; p < total; p++) {
        uint16_t c = buf[p];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        buf[p] = ((r * 15 / 100) << 11) | ((g * 15 / 100) << 5) | (b * 15 / 100);
    }

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
    canvas.setTextColor(theme.dim);
    char slotStr[8];
    snprintf(slotStr, sizeof(slotStr), "P%02d", _currentProjectSlot + 1);
    canvas.drawString(slotStr, SCREEN_WIDTH - 8, 6);
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
    }
    getView(_currentScreen)->exit();
    _currentScreen = s;
    getView(_currentScreen)->enter();
}
