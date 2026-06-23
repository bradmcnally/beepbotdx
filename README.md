# beepbot dx

A handheld sampler and tracker-style sequencer for the M5Stack Cardputer ADV (ESP32-S3).

Record sounds with the built-in mic, load WAV files from SD, build patterns, arrange songs, and perform — all from a pocket-sized device with a tiny keyboard and a 240x135px display.

## Features

- **8 sound slots** — record via microphone or import WAV from SD card
- **16-step sequencer** — 16 patterns, each with 8 tracks
- **Song mode** — chain up to 16 patterns
- **Live play** — trigger sounds and patterns in real time
- **Waveform trim** — visual start/end point editing
- **Bloom visualization** — reactive wave/ripple field driven by audio
- **Character companion** — animated face that reacts to your actions
- **Theme system** — multiple color palettes
- **Project save/load** — persistent storage on SD card
- **Screenshot capture** — save PNGs to SD card
- **Desktop simulator** — full SDL2 build for development

## Architecture

```
src/
├── main.cpp                 # Hardware entry point (M5Stack)
├── config.h                 # Constants (sample rate, grid size, display)
├── core/                    # Platform-independent logic
│   ├── sequencer            # Step sequencer + song playback
│   ├── project              # Data model (sounds, patterns, songs)
│   ├── bloom_field          # Visual bloom/ripple simulation
│   ├── character            # Companion face + message system
│   ├── theme                # Color palette presets
│   ├── canvas               # Drawing abstraction
│   └── sound_slot           # Audio sample container
├── views/                   # Screen implementations
│   ├── sound_view           # Record, trim, rename, list
│   ├── pattern_select_view  # Pattern list + copy/paste
│   ├── pattern_edit_view    # Step grid editor
│   ├── song_view            # Song arrangement
│   ├── play_view            # Live performance + bloom
│   └── project_view         # Save/load/theme picker
├── platform/                # Hardware HAL (M5Stack Cardputer)
│   ├── audio, display, input, storage, power, memory, screenshot
└── platform_desktop/        # Desktop HAL (SDL2)
    ├── main_desktop.cpp     # Desktop entry point
    ├── audio, display, input, storage, power, memory
```

The two main files (`main.cpp` and `platform_desktop/main_desktop.cpp`) share identical event loops and rendering logic. Platform differences are isolated to the HAL layer.

## Building

### Hardware (M5Stack Cardputer ADV)

Requires [PlatformIO](https://platformio.org/).

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
```

Produces `beep.bin` for flashing.

### Desktop Simulator

Requires SDL2 and SDL2_image.

```sh
# macOS
brew install sdl2 sdl2_image

# Build
cmake -B build
cmake --build build

# Run
./build/beepbotdx
```

### Tests

```sh
pio test -e native
```

## Controls

Navigation uses a tab menu (hold TAB + arrows or tap to cycle).

| Key | Action |
|-----|--------|
| TAB | Navigate screens |
| S | Save project |
| O | Open project |
| +/- | Volume |
| B+/- | BPM |
| Fn+/- | Brightness |
| H | Help overlay |
| SPACE | Play/audition |
| ENTER | Select/confirm |
| 1-8 | Trigger sounds |

Press **H** on any screen for context-sensitive keyboard shortcuts.

## Constraints

- 240x135px display, 5x7 bitmap font
- 16kHz mono audio, 2 second max sample length
- 8 sound slots, 16 patterns, 16 steps, 16 song positions
- 4-voice polyphony
- BPM range: 60-240

## License

All rights reserved.
