# beepBot DX

A tiny sampler and sequencer for M5Stack Cardputer/ADV/Zero.

Record sounds with the built-in mic, import WAV files from SD, build patterns, arrange songs, perform, and export.

![Sound Slots](assets/sound_slots.png)
![Trim](assets/trim.png)
![FX](assets/fx.png)
![Sequencer](assets/sequencer.png)
![Playback 1](assets/play.png)

## Features

- **8 projects** with color themes and renameable slots
- **8 sound slots** — record hands-free, push-to-record, or import WAV from SD (16kHz mono, 2s max, trimmable)
- **Per-slot FX** — pitch shift, bitcrush, low-pass and high-pass filters
- **16-step sequencer** — 16 patterns, 8 tracks, 8-voice polyphony, copy/paste patterns
- **Song mode** — chain up to 16 patterns
- **Live play** — trigger sounds and patterns with playback visualization
- **60-240 BPM** per-project tempo with LED sync (StampS3A)
- **WAV export** — render full song to file on SD
- **`\(^_^)/`** —  beepb0t reacts to your actions and provides feedback
- **Settings** — auto-save, LED mode, confirm delete, boot to project/list

## Workflow

1. Record or import sounds → trim to tighten → add effects
2. Build patterns on the 16-step grid
3. Chain patterns into a song
4. Listen to the song and vibe with the visualization
5. Export the song to WAV on SD

## Controls

| Key | Action |
|-----|--------|
| TAB | Navigate screens |
| S | Save project |
| O | Open project |
| G | Global settings |
| P | Project settings |
| M | Cycle LED mode |
| +/- | Volume |
| B+/- | BPM |
| N+/- | Brightness |
| H | Help overlay |
| F | Table flip |
| SPACE | Play/audition |
| CTRL/OK | Select/confirm |
| 1-8 | Trigger sounds |

Navigation uses a tab menu (hold TAB + arrows or tap to cycle).
 
![Menu](assets/menu.png)

Press **H** on any screen for context-sensitive keyboard shortcuts. Press **G** for settings. Press **P** for project info.

![Help](assets/help.png)
![Help](assets/settings.png)
![Help](assets/projectsettings.png)

### Sound
![Sounds](assets/sounds.png)
![Recording](assets/recording.png)
![Import](assets/import.png)
| Key | Action |
|-----|--------|
| CTRL/OK | Edit/Record |
| G0 | Push-to-record into focused slot |
| SPACE | Audition |
| DEL | Clear |
| I | Import WAV |
| T | Trim focused sound |
| F | Add FX to focused out |
| R | Rename |
| 1-8 | Audition slot |

### Trim
![Trim 1](assets/trim_1.png)
![Trim 2](assets/trim_2.png)
| Key | Action |
|-----|--------|
| L/R | Adjust trim point |
| U/D | Switch between start/end point |
| SPACE | Audition |
| +/- | Volume |
| L +/- | Level |
| CTRL/OK | Apply |
| F | Switch to FX |
| ESC | Cancel |

### FX
![FX](assets/fx.png)
| Key | Action |
|-----|--------|
| L/R | Select effect |
| U/D | Adjust value |
| CTRL/OK | Toggle on/off |
| SPACE | Audition |
| DEL | Reset to default |
| T | Switch to trim |
| L +/- | Level |
| ESC | Back |

### Pattern Select
![Pattern List](assets/pattern_list.png)
| Key | Action |
|-----|--------|
| CTRL/OK | Edit pattern |
| SPACE | Audition |
| DEL | Clear |
| Fn+C | Copy |
| Fn+V | Paste |

### Pattern Edit
![Sequencer 1](assets/sequencer.png)
![Sequencer 2](assets/sequencer_2.png)
![Sequencer 3](assets/sequencer_3.png)
| Key | Action |
|-----|--------|
| CTRL/OK | Toggle step |
| SPACE | Play/stop |
| ESC | Back |
| 1-8 | Audition |
| Fn+1-8 | Live record |

### Song
![Pattern chain](assets/pattern_chain.png)
| Key | Action |
|-----|--------|
| CTRL/OK | Edit pattern |
| SPACE | Play song |
| DEL | Clear slot |
| [ ] | Cycle pattern in slot |
| E | Export WAV |

### Play
![Playback 1](assets/play.png)
![Playback 2](assets/playback_2.png)
![Playback 3](assets/playback_3.png)
| Key | Action |
|-----|--------|
| SPACE | Play/stop |
| 1-8 | Audition |
| E | Export WAV |

## SD Card

Place WAV files for import in `/beepbotdx/samples/` on the SD card. Files should be 16kHz mono.

## Memory & Samples

The Cardputer ADV has ~127 KB available for samples — roughly **4 seconds total** shared across all 8 slots (16-bit mono @ 16kHz = 32 KB/s). Recording adapts to available memory, so shorter existing samples leave more room for new ones. Trim recordings and clear unused slots to free space. Press **P** from any screen to see memory usage per slot.

The Cardputer Zero has significantly more memory and  beepBot DX will support up to 10 seconds per sample.

## Planned Features

- Swing
- 8-bit sample mode for longer recording time (4s -> 8s total)
- More visualizations

![Zero](assets/beepbotdx_zero.png)

## Architecture

```
src/
├── main.cpp                 # Hardware entry point (M5Stack)
├── config.h                 # Constants (sample rate, grid size, display)
├── core/                    # Platform-independent logic
│   ├── app                  # Main application controller
│   ├── sequencer            # Step sequencer + song playback
│   ├── project              # Data model (sounds, patterns, songs)
│   ├── bloom_field          # Visual bloom/ripple simulation
│   ├── character            # Companion face + message system
│   ├── theme                # Color palette presets
│   ├── canvas               # Drawing abstraction
│   └── sound_slot           # Audio sample container
├── views/                   # Screen implementations
│   ├── sound_view           # Record, trim, rename, import, list
│   ├── pattern_select_view  # Pattern grid + copy/paste/clear
│   ├── pattern_edit_view    # Step grid editor + live record
│   ├── song_view            # Song arrangement with playhead
│   ├── play_view            # Live performance + bloom
│   ├── project_view         # Save/load/delete (2x4 grid)
│   └── settings_view        # Auto-save, LED, warnings
├── platform/                # Hardware HAL (M5Stack Cardputer)
│   ├── audio, display, input, storage, power, memory, led, screenshot
└── platform_desktop/        # Desktop HAL (SDL2)
    ├── main_desktop.cpp     # Desktop entry point
    ├── audio, display, input, storage, power, memory
```

The two main files (`main.cpp` and `platform_desktop/main_desktop.cpp`) share event loops and rendering logic. Platform differences are isolated to the HAL layer.

## Building

### Hardware (M5Stack Cardputer/ADV/Zero)

Requires [PlatformIO](https://platformio.org/).

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
```

### Desktop Simulator

Requires SDL2 and SDL2_image.

```sh
# macOS
brew install cmake pkg-config sdl2 sdl2_image

# Build (Cardputer / ADV, 240x135)
cmake --preset adv && cmake --build build && ./build/beepbotdx

# Build (Zero, 320x170)
cmake --preset zero && cmake --build build_zero && ./build_zero/beepbotdx
```

### Tests

```sh
pio test -e native
```