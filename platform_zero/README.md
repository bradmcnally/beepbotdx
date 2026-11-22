# beepBot DX on Cardputer Zero

The Zero runs the 320×170 Linux build. It writes RGB565 frames to the built-in LCD, reads the keypad through Linux input events, and uses SDL2 for the speaker and microphone. [M5Stack peripheral guide](https://github.com/CardputerZero/Template/wiki/Peripheral-Development-Guide)

## Build and install

Run these commands from the beepBot DX source folder on the Zero.

```sh
sudo apt install build-essential cmake pkg-config libsdl2-dev
cmake --preset zero-device
cmake --build --preset zero-device
cpack --config build_zero_device/CPackConfig.cmake -B build_zero_device
sudo dpkg -i build_zero_device/beepbotdx_0.6.0_arm64.deb
```

Open **beepBot DX** from HOME. You can also run `./build_zero_device/beepbotdx` as the regular device user. The splash screen lasts one second; tap Esc to skip it. Hold **Esc** for three seconds to return HOME, or press **Q**. Release Esc early to cancel the countdown. If the project has unsaved changes, choose Save or Discard at the prompt.

If HOME cannot start the app, check `~/.local/state/beepbotdx/launch.log` or the corresponding folder under `$XDG_STATE_HOME`. The launch user needs write access to `/dev/fb_lcd` (or `/dev/fb0`), read access to the keypad input device, and write access to the system backlight control for brightness changes. The log also reports playback and recording device errors.

## Keyboard

| Keys | Action |
| --- | --- |
| **F / Z / X / C** | Up / Left / Down / Right. Fn+F/Z/X/C also works. In a name field, the plain keys type letters. |
| **E** | Open FX from Sound or Trim. Export WAV from Song, Play, or Projects. |
| **K** | Flip the table. |
| **Tab** or **Tab+F / Z / X / C** | Tap Tab to cycle screens, or hold it and choose a screen with the letter keys. |
| **Enter**, **Esc** | Select or confirm; tap Esc to go back or cancel. Hold Esc for three seconds to return HOME. |
| **Backspace** or **Fn+Backspace** | Clear a slot where the screen offers Delete. |
| **H** or **Fn+H** | Open contextual Help. |
| **S**, **O**, **G**, **P** | Save, browse projects, open global settings, or open the current project's settings and summary. |
| **Space**, **1–8** | Play or audition. |
| **Fn+B** | Hold to record in the Sound slot grid. Release to stop. |
| **Ctrl+C / Ctrl+V** | Copy or paste in Pattern Select. |
| **Fn+1–8** | Live record a sound while a pattern plays in Pattern Edit. |
| **Fn+K / Fn+N** | Cycle the selected pattern in Song. |
| **Fn+S / Fn+D** | Lower or raise the Zero's system volume on every screen. The setting is shared with HOME. |
| **Ctrl+J / Ctrl+K** | Lower or raise the selected sample's level in Trim and FX. |
| **Fn+U / Fn+I** | Dim or brighten the backlight. The header shows the new percentage. |
| **Q** | Quit. |

At the unsaved-changes prompt, press **S** to save, **D** to discard, or **Esc** to cancel. The Zero's Fn key sends device-specific key codes. In the simulator, use the desktop arrow keys, F1–F8, Insert, Home, and End for the corresponding functions. [M5Stack keyboard guide](https://docs.m5stack.com/en/compute/cardputerzero/usage#2-keyboard-operation)

The Zero's **Sym** key sends dedicated key codes for its printed punctuation. Project and sound names accept those characters, including `-`, `.`, and `/`.

## Projects and WAV files

The red **ACT indicator** flashes on each beat during playback when **METRONOME** is **ON**. Choose **OFF** to disable it, or press **M** to toggle it. The indicator stays off when playback stops; project colors and battery warnings do not light it. Existing On and Metronome settings migrate to ON. The app restores the indicator's previous brightness and kernel trigger when it exits normally. The launch user needs write access to `/sys/class/leds/ACT/brightness` and `/sys/class/leds/ACT/trigger`. The device package installs a udev rule granting that access to the `input` group, which also accesses the keypad. Package installation applies those permissions immediately and reloads the udev rules; no reboot is needed. If control is unavailable, the app logs the problem and continues without the LED. This uses the kernel LED interface described in the [peripheral guide](https://github.com/CardputerZero/Template/wiki/Peripheral-Development-Guide), rather than direct I²C access.

On the Project screen, edit the current project's name, color, new-sample bit depth, and BPM. The summary shows occupied sounds and patterns plus sample memory use. Select **Save** or **Export WAV** at the bottom; an asterisk beside the name means the project has unsaved changes.

The Zero stores projects, settings, and WAV exports in `~/.local/share/beepbotdx`, or `$XDG_DATA_HOME/beepbotdx` if set. Importable WAV files go in the visible `~/Music/beepbotdx/samples/` folder, which the app creates on launch. Set `BEEPBOTDX_DATA_DIR` to use a self-contained workspace; its `samples/` folder is used for imports instead.

- Press **I** on the Sound screen to open the file browser, which starts in `~/Music/beepbotdx/samples/`. Copy WAVs there for quick access, or navigate to another folder. Use up/down to select, Enter/right to open a folder or import a WAV, left/backspace to go to the parent folder, Space to preview a WAV, and Esc to close the browser.
- Selecting an empty sound slot opens a Record / Import menu with Record focused. Move right to Import and press Enter, or use **R** and **I** to choose either action directly.
- To move a saved project from the desktop simulator or an ADV SD card, copy its numbered project folder (such as `01/`) into the Zero workspace. Keep the project's `project.dat` and its sound WAVs together.
- Press **E** in Song or Play to export the song as a WAV in the workspace.

## Normalize

Select a sound and press **N** to toggle Normalize, or use **N** from Trim or FX. The footer's **N** is dim when off and uses the full accent color when on; the message area confirms “normalize on” or “normalize off.” Normalize boosts the sample's highest peak to the digital ceiling during playback and song export. It works with microphone recordings and imported WAVs, leaves Level unchanged, and saves with the project. Turning it off restores the original volume because the stored audio is unchanged. Silent samples remain silent. Recordings now use the original microphone signal instead of the previous recording gain control.
