# 🎵 neoDAW

A modern, lightweight Digital Audio Workstation (DAW) built in C++20. It combines a classic step sequencer, a high-resolution piano roll, a multi-track playlist, and an 8-channel mixer with a real-time Wavetable and SoundFont synthesis engine.

---

## ✨ Features

* **Real-Time Sound Synthesis:**
  * **Wavetable Synth:** Custom synthesizer supporting Sine, Saw, Square, Triangle, and Noise waveforms with an ADSR envelope.
  * **SoundFont Player:** Integrated **FluidLite** engine for real-time playbacks of standard `.sf2` instrument patches.
* **Sequencing & Composition:**
  * **Step Sequencer:** 16-step grid for quick drum/synth programming.
  * **Piano Roll:** High-resolution note editor with pitch adjustments, velocity control, length resizing, and grid snapping (1/4, 1/8, etc.).
  * **Playlist:** Arrangement board to map patterns across multiple tracks with vertical/horizontal scrolling and zooming.
* **Mixing & DSP Effects:**
  * **Mixer Board:** 8-channel stereo mixer with volume, panning, muting, and soloing.
  * **Stereo Feedback Delay:** Custom per-channel feedback delay effect with configurable delay time, feedback depth, and wet mix.
* **File I/O & Exporting:**
  * **Project Files:** JSON-based state serialization for project configurations (`.neodaw`).
  * **Audio Rendering:** Export projects directly to standard stereo `.wav` audio.
  * **MIDI Support:** Export sequences as `.mid` files or import external MIDI loops directly into sequencer tracks.

---

## 🛠️ Technology Stack

* **Language:** C++20 (features robust STL utilization)
* **Graphics & Windowing:** OpenGL 3.3 & SDL2
* **GUI Engine:** Dear ImGui (Docking branch `v1.92.8`)
* **Sound Synthesis:** FluidLite (SoundFont Synth)
* **Serialization:** nlohmann/json (v3.11.3)

---

## 🚀 Building and Running

### Prerequisites
* **CMake** (v3.20 or newer)
* **C++20 Compiler:** MSVC (Visual Studio 2022/2026 recommended), GCC, or Clang.

> [!IMPORTANT]
> This project uses CMake's `FetchContent` to download dependencies. The first configuration step requires an active internet connection to download SDL2, ImGui, FluidLite, and nlohmann/json.

### Build Steps

1. **Configure the Project:**
   ```bash
   cmake -B build
   ```

2. **Compile the DAW:**
   ```bash
   cmake --build build --config Release
   ```

3. **Deploy & Launch (Windows MSVC):**
   ```powershell
   # Copy the generated SDL2 DLL to the runtime directory
   Copy-Item -Path "build\_deps\sdl2-build\Release\SDL2.dll" -Destination "build\Release\"

   # Run the executable
   .\build\Release\neodaw.exe
   ```

---

## ⌨️ Shortcuts & Controls

| Action | Shortcut / Control | Location |
| :--- | :--- | :--- |
| **Play / Pause** | `Spacebar` | Global |
| **Stop & Reset** | `Escape` | Global |
| **Undo Action** | `Ctrl + Z` | Global |
| **Redo Action** | `Ctrl + Y` | Global |
| **Draw / Move Notes**| `Left Click + Drag` | Piano Roll |
| **Resize Notes** | `Drag note edges` | Piano Roll |
| **Select Pattern** | `Left Click` | Playlist / Channel Rack |
