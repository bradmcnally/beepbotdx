#include <SDL2/SDL.h>
#include "config.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "core/character.h"
#include "core/sound_slot.h"
#include "core/project.h"
#include "core/sequencer.h"
#include "core/timing.h"
#include "views/view.h"
#include "views/sound_view.h"
#include "views/pattern_select_view.h"
#include "views/pattern_edit_view.h"
#include "views/song_view.h"
#include "views/play_view.h"
#include "views/project_view.h"
#include "core/theme.h"
#include "platform/power.h"

static Project project;
static Character character;
static Sequencer sequencer;
static uint8_t currentProjectSlot = 0;

static SoundView soundView(project, character);
static PatternSelectView patternSelectView(project, character, sequencer);
static PatternEditView patternEditView(project, character, sequencer);
static SongView songView(project, character, sequencer);
static PlayView playView(project, character, sequencer);
static ProjectView projectView(project, character, currentProjectSlot);

enum Screen {
    SCREEN_SOUND,
    SCREEN_PATTERN_SELECT,
    SCREEN_PATTERN_EDIT,
    SCREEN_SONG,
    SCREEN_PLAY,
    SCREEN_PROJECT,
};

static Screen currentScreen = SCREEN_SOUND;
static Screen screenBeforeProject = SCREEN_SOUND;

static View* getView(Screen s) {
    switch (s) {
        case SCREEN_SOUND: return &soundView;
        case SCREEN_PATTERN_SELECT: return &patternSelectView;
        case SCREEN_PATTERN_EDIT: return &patternEditView;
        case SCREEN_SONG: return &songView;
        case SCREEN_PLAY: return &playView;
        case SCREEN_PROJECT: return &projectView;
    }
    return &soundView;
}

static void switchScreen(Screen s) {
    getView(currentScreen)->exit();
    currentScreen = s;
    getView(currentScreen)->enter();
}

static void onTrigger(uint8_t soundIndex) {
    if (soundIndex < NUM_SOUNDS && project.sounds[soundIndex].occupied) {
        SoundSlot& slot = project.sounds[soundIndex];
        Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100);
    }
    character.setState(CHAR_BEAT);
    playView.onTrigger(soundIndex);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();

    Project::init(project);
    sequencer.init(&project);
    sequencer.setCallback(onTrigger);

    Storage::loadProject(project, currentProjectSlot);

    getView(currentScreen)->enter();

    // Tab menu state
    bool tabMenuOpen = false;
    bool tabMenuVisible = false;
    bool tabMenuMoved = false;
    uint32_t tabPressTime = 0;
    uint8_t tabMenuCursor = 0;
    const Screen tabMenuScreens[] = { SCREEN_SOUND, SCREEN_PATTERN_SELECT, SCREEN_SONG, SCREEN_PLAY };
    const char* tabMenuLabels[] = { "SOUND", "PATTERN", "SONG", "PLAY" };
    const int tabMenuCount = 4;
    const uint32_t TAB_HOLD_THRESHOLD = 200;

    bool running = true;
    while (running) {
        character.tick();

        if (Audio::isRecording()) {
            Audio::recordUpdate();
        }

        sequencer.tick(millis());

        InputEvent event = Input::poll();

        // 0 key saves project
        if (event == INPUT_NUM0 && currentScreen != SCREEN_PROJECT) {
            character.setState(CHAR_SAVING);
            character.say("saving...");
            if (Storage::saveProject(project, currentProjectSlot)) {
                character.setState(CHAR_SUCCESS);
                character.say("saved!");
            } else {
                character.setState(CHAR_ERROR);
                character.say("save failed");
            }
            event = INPUT_NONE;
        }

        // 9 key opens project picker
        if (event == INPUT_NUM9 && currentScreen != SCREEN_PROJECT) {
            screenBeforeProject = currentScreen;
            switchScreen(SCREEN_PROJECT);
            event = INPUT_NONE;
        }

        // +/- volume, B+plus/minus BPM
        if ((event == INPUT_PLUS || event == INPUT_MINUS) && currentScreen != SCREEN_PROJECT
            && !(currentScreen == SCREEN_SOUND && soundView.inTrim())) {
            if (Input::isBHeld()) {
                if (event == INPUT_PLUS && project.bpm < MAX_BPM) project.bpm++;
                if (event == INPUT_MINUS && project.bpm > MIN_BPM) project.bpm--;
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
                character.say(volMsg);
            }
            event = INPUT_NONE;
        }

        // TAB menu: tap to cycle, hold+arrows to pick
        if (event == INPUT_TAB && !tabMenuOpen && currentScreen != SCREEN_PROJECT) {
            tabMenuOpen = true;
            tabMenuVisible = false;
            tabMenuMoved = false;
            tabPressTime = SDL_GetTicks();
            for (int i = 0; i < tabMenuCount; i++) {
                if (tabMenuScreens[i] == currentScreen ||
                    (tabMenuScreens[i] == SCREEN_PATTERN_SELECT && currentScreen == SCREEN_PATTERN_EDIT)) {
                    tabMenuCursor = i;
                    break;
                }
            }
            event = INPUT_NONE;
        } else if (tabMenuOpen) {
            if (!Input::isTabHeld()) {
                bool wasVisible = tabMenuVisible;
                tabMenuOpen = false;
                tabMenuVisible = false;
                if (tabMenuMoved) {
                    if (tabMenuScreens[tabMenuCursor] != currentScreen) {
                        switchScreen(tabMenuScreens[tabMenuCursor]);
                    }
                } else if (!wasVisible) {
                    uint8_t next = (tabMenuCursor + 1) % tabMenuCount;
                    switchScreen(tabMenuScreens[next]);
                }
            } else {
                if (!tabMenuVisible && (SDL_GetTicks() - tabPressTime >= TAB_HOLD_THRESHOLD)) {
                    tabMenuVisible = true;
                }
                if (event == INPUT_LEFT || event == INPUT_UP) { tabMenuCursor = (tabMenuCursor + tabMenuCount - 1) % tabMenuCount; tabMenuMoved = true; }
                if (event == INPUT_RIGHT || event == INPUT_DOWN) { tabMenuCursor = (tabMenuCursor + 1) % tabMenuCount; tabMenuMoved = true; }
            }
            event = INPUT_NONE;
        }

        View* view = getView(currentScreen);
        view->update(event);

        if (currentScreen == SCREEN_PROJECT) {
            if (projectView.shouldClose()) {
                projectView.clearClose();
                switchScreen(screenBeforeProject);
            } else if (projectView.didLoad()) {
                projectView.clearLoad();
                switchScreen(SCREEN_SOUND);
            }
        }

        if (currentScreen == SCREEN_PATTERN_SELECT && patternSelectView.shouldEditPattern()) {
            patternSelectView.clearEditRequest();
            patternEditView.setPattern(patternSelectView.getSelectedPattern());
            switchScreen(SCREEN_PATTERN_EDIT);
        }

        if (currentScreen == SCREEN_PATTERN_EDIT && patternEditView.shouldGoBack()) {
            patternEditView.clearBackRequest();
            switchScreen(SCREEN_PATTERN_SELECT);
        }

        if (currentScreen == SCREEN_SONG && songView.shouldEditPattern()) {
            songView.clearEditRequest();
            patternEditView.setPattern(songView.getEditPattern());
            switchScreen(SCREEN_PATTERN_EDIT);
        }

        Display::beginFrame();
        Canvas& canvas = Display::canvas();

        Theme theme = ThemeOps::getPreset(project.themeIndex);
        canvas.fillScreen(theme.bg);

        // Header bar (centered to match grid)
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
        switch (currentScreen) {
            case SCREEN_SOUND:
                if (soundView.inSubView()) {
                    snprintf(titleBuf, sizeof(titleBuf), "SND %02d", soundView.getCursor() + 1);
                    title = titleBuf;
                } else {
                    title = "SOUND";
                }
                break;
            case SCREEN_PATTERN_SELECT:
                title = "PATTERN";
                break;
            case SCREEN_PATTERN_EDIT:
                snprintf(titleBuf, sizeof(titleBuf), "PTRN %02d", patternEditView.getPattern() + 1);
                title = titleBuf;
                break;
            case SCREEN_SONG:
                title = "SONG";
                break;
            case SCREEN_PLAY:
                title = "PLAY";
                break;
            case SCREEN_PROJECT:
                title = "PROJECT";
                break;
        }
        canvas.setTextDatum(top_left);
        canvas.drawString(title, hdrLeft + 4, 7);

        // Character face (center)
        const char* face = character.getFace();
        canvas.setTextDatum(top_center);
        canvas.drawString(face, hdrLeft + hdrContentW / 2, 7);

        // Character message (right of face)
        const char* msg = character.getMessage();
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

        view->draw(canvas);

        // Tab menu overlay
        if (tabMenuVisible) {
            // Scrim: darken all pixels to ~15% brightness
            uint16_t* buf = canvas.buffer();
            int total = SCREEN_WIDTH * SCREEN_HEIGHT;
            for (int p = 0; p < total; p++) {
                uint16_t c = buf[p];
                uint8_t r = (c >> 11) & 0x1F;
                uint8_t g = (c >> 5) & 0x3F;
                uint8_t b = c & 0x1F;
                buf[p] = ((r * 15 / 100) << 11) | ((g * 15 / 100) << 5) | (b * 15 / 100);
            }

            // Vertical list, centered
            const int itemH = 16 + 10; // text height + spacing
            const int totalH = tabMenuCount * 16 + (tabMenuCount - 1) * 10;
            int startY = (SCREEN_HEIGHT - totalH) / 2;

            canvas.setTextSize(2);
            canvas.setTextDatum(top_left);
            for (int i = 0; i < tabMenuCount; i++) {
                int y = startY + i * itemH;
                if (i == tabMenuCursor) {
                    canvas.setTextColor(TFT_WHITE);
                    canvas.drawString(">", 14, y);
                    canvas.drawString(tabMenuLabels[i], 30, y);
                } else {
                    canvas.setTextColor(theme.accent);
                    canvas.drawString(tabMenuLabels[i], 30, y);
                }
            }
        }

        Display::endFrame();
        SDL_Delay(10);
    }

    Display::shutdown();
    return 0;
}
