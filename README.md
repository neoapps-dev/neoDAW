# neoDAW

A weird digital audio workstation made by a weird guy.

C++20, SDL2 + OpenGL 3.3, Dear ImGui (docking branch), JUCE 8 for audio I/O, FluidLite for SoundFont playback, custom synth engine. No VST support or recording yet!

> [!WARNING]
> neoDAW is **experimental software**. I can't even consider it pre-alpha yet. So, be sure to report bugs early and submit pull requests.

## Building

### Linux

```
sudo apt-get install libsdl2-dev libasound2-dev libjack-jackd2-dev \
  ladspa-sdk libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev \
  mesa-common-dev

cmake -S . -B build
cmake --build build
```

### Windows

MSVC via CMake. vcpkg for SDL2 and curl. CI confirms it works.

### macOS

Not supported. No CoreAudio backend, no Cocoa file dialogs, no one to write them. Unless you want to add those by yourself and contribute of course!

## Dependencies

All pulled at configure time via `FetchContent` except SDL2, OpenGL, and libcurl:

| Library | Tag | Why |
|---------|-----|-----|
| JUCE | 8.0.6 | Audio device callback, WAV reading/writing, DSP blocks for reverb and chorus |
| Dear ImGui | v1.92.8-docking | All the UI. The docking branch lets you rearrange panels. |
| FluidLite | master | Lightweight FluidSynth, loads SF2 files, renders MIDI to samples |
| midifile | master | Reads and writes Standard MIDI Files |
| nlohmann/json | v3.11.3 | Serializes project files to JSON |

## Data model

Everything lives in a `Project` struct defined in [`src/Types.h`](src/Types.h). It holds vectors of channels, patterns, mixer slots, and playlist clips. Channels can be one of three types:

- **Synth**: built-in synthesizer with 5 waveforms (sine/saw/square/triangle/noise), ADSR envelope, resonant state-variable filter (LP/HP/BP)
- **Sampler**: plays back WAV files with optional looping
- **SF2**: SoundFont 2 playback via FluidLite

Patterns hold notes (key, velocity, start tick, length). Mixer slots (fixed at 8) hold volume, pan, mute, and per-slot effect parameters. Playlist clips reference a pattern and a track, with start position and length in ticks.

The project file is plain JSON. You can open `projects/demo.neodaw` in any text editor and see exactly what's going on lmao.

## Audio engine (`src/AudioEngine.cpp`)

The audio callback runs on a separate thread via JUCE's `AudioIODeviceCallback`. It calls a `render()` function each buffer that:

1. Figures out what to play, either the playlist arrangement, a single pattern (for editing), or the step sequencer grid
2. For each active channel, generates samples through the appropriate path:
   - **Synth** -> routes MIDI note events to a pre-allocated pool of 16 `Synthesizer` instances (one per channel, 32 voices each, round-robin voice stealing)
   - **Sampler** -> reads WAV data, applies ADSR as an amplitude envelope, loops if enabled
   - **SF2** -> sends note on/off to FluidLite, reads its output buffer
3. Sums per-channel output into the corresponding mixer slot
4. Processes each mixer slot's effects chain in order: distortion -> delay -> filter -> limiter -> reverb -> chorus
5. Accumulates into the master bus where there's another filter, limiter, and volume control
6. Mixes in the metronome (a decaying sine tone. 1200 Hz on downbeats, 800 Hz otherwise)

Most effects run per-sample in the inner loop. Reverb and chorus use JUCE's DSP modules which expect block processing, so those get fed whole buffer chunks.

Thread safety is a single `std::mutex`. The UI thread locks it when modifying project data. The audio callback tries `try_lock()`, if the UI thread is holding it, the callback fills the buffer with silence and returns. This avoids priority inversion but means a busy UI causes audio dropouts.

The synth engine (`src/Synth.h`) is a single inline header lmfao. No separate compilation unit. 32 voices, each with a phase accumulator, ADSR state machine, and one-sample filter memory. No lookup tables, no SIMD. Just straightforward per-sample math.

## MIDI import/export

Import reads an SMF via midifile, groups events by MIDI channel, and creates one channel plus one pattern per channel that has notes. Minimum pattern length is one bar (4 beats at current PPQ). Program changes map to SF2 presets if a SoundFont is loaded.

Export writes Format 0 (single track) SMF at PPQ 480.

## Undo/redo

Snapshots the entire `Project` struct into a stack on every mutation. Max 50 undo levels. No diffing, command pattern, structural sharing or whatever. `undo()` swaps the current project with the top of the undo stack, pushes the old project onto the redo stack. Simple AF lmao. Memory-hungry for large projects. Works fine for normal use.

## File dialogs

- **Windows**: `GetOpenFileNameA` / `GetSaveFileNameA`
- **Linux**: `zenity --file-selection` via `popen`
- **macOS**: falls through to nothing

## CLI

```
./neodaw                          # GUI
./neodaw project.neodaw           # GUI, load project
./neodaw --render-debug project   # Headless, renders to debug_*.wav files
```

The `--render-debug` flag skips all UI initialization. It writes four WAV files with different render paths to help isolate audio bugs.

## Hotkeys

| Key | Action |
|-----|--------|
| Ctrl+N/O/S | New / Open / Save |
| Ctrl+Shift+S | Save as |
| Ctrl+Z / Ctrl+Shift+Z | Undo / Redo |
| Ctrl+C/V/X | Copy / Paste / Cut notes |
| Ctrl+A | Select all notes |
| Ctrl+D | Duplicate selected |
| Space | Toggle play/pause |
| Q | Toggle metronome |
| Delete | Remove selected notes |
| Arrows | Nudge (Shift=octave, Ctrl=bar) |
| Alt+click | Duplicate note under cursor |
| Ctrl+click | Move playhead |

## Stuff that umm aren't that good

- `try_lock` means audio glitches if the UI thread is busy
- No recording of any kind
- No VST/AU plugins (yet!). Effects are all built-in and hardcoded
- 8 mixer slots, compile-time constant, not configurable
- Step sequencer: one note per step at C5, 16 steps, that's it
- Undo copies everything.
- One FluidLite instance, one SoundFont at a time
- Only Windows and GNU/Linux
- libcurl is linked but never actually used (leftover from an earlier idea)
- No test suite lmfao, I hate those. `--render-debug` is as close as it gets

## License

neoDAW is licensed under the [MPL-2.0](LICENSE) license.
