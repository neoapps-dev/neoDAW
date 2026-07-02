#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "App.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>
static const ImU32 COL_BG = IM_COL32(30, 30, 30, 255);
static const ImU32 COL_BG2 = IM_COL32(45, 45, 45, 255);
static const ImU32 COL_BG3 = IM_COL32(55, 55, 55, 255);
static const ImU32 COL_ACCENT = IM_COL32(255, 170, 0, 255);
static const ImU32 COL_ACCENT2 = IM_COL32(255, 200, 50, 255);
static const ImU32 COL_TEXT = IM_COL32(220, 220, 220, 255);
static const ImU32 COL_TEXT_DIM = IM_COL32(140, 140, 140, 255);
static const ImU32 COL_NOTE = IM_COL32(68, 180, 255, 200);
static const ImU32 COL_NOTE_ACTIVE = IM_COL32(100, 220, 255, 255);
static const ImU32 COL_GRID_LINE = IM_COL32(60, 60, 60, 255);
static const ImU32 COL_GRID_BEAT = IM_COL32(75, 75, 75, 255);
static const ImU32 COL_GRID_SNAP = IM_COL32(60, 70, 60, 255);
static const ImU32 COL_KEY_WHITE = IM_COL32(200, 200, 200, 255);
static const ImU32 COL_KEY_BLACK = IM_COL32(40, 40, 40, 255);
static const ImU32 COL_PLAYLIST_CLIP = IM_COL32(68, 180, 255, 150);
static const ImU32 COL_MIXER_FADER = IM_COL32(255, 170, 0, 255);
static const ImU32 COL_SELECTION = IM_COL32(255, 255, 100, 60);
static const ImU32 COL_MUTED = IM_COL32(200, 60, 60, 255);
static const ImU32 COL_SOLOED = IM_COL32(255, 220, 60, 255);
static const int UNDO_MAX = 50;
static int noteNameToKey(const char* name) {
    const char* notes[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    for (int i = 0; i < 12; i++) {
        if (strncmp(name, notes[i], strlen(notes[i])) == 0)
            return i;
    }
    return 0;
}

static const char* keyToNoteName(int key) {
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return names[key % 12];
}

void renderTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(6, 6);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 12.0f;
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 0.80f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.90f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 0.90f);
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.67f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.78f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.67f, 0.00f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.67f, 0.00f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
}

void setStatus(AppState& state, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(state.statusMessage, sizeof(state.statusMessage), fmt, args);
    va_end(args);
    state.statusTimer = 5.0f;
}

void pushUndo(AppState& state) {
    state.undoStack.push_back(state.project);
    if (state.undoStack.size() > (size_t)UNDO_MAX)
        state.undoStack.erase(state.undoStack.begin());
    state.redoStack.clear();
}

void undo(AppState& state) {
    if (state.undoStack.empty()) return;
    state.redoStack.push_back(state.project);
    state.project = state.undoStack.back();
    state.undoStack.pop_back();
    state.project.modified = true;
    if (state.engine) state.engine->stopPlayback();
}

void redo(AppState& state) {
    if (state.redoStack.empty()) return;
    state.undoStack.push_back(state.project);
    state.project = state.redoStack.back();
    state.redoStack.pop_back();
    state.project.modified = true;
    if (state.engine) state.engine->stopPlayback();
}

bool appInit(AppState& state, AudioEngine* engine) {
    state.engine = engine;
    state.project.ensureMixerSlots();
    if (state.project.channels.empty()) {
        Channel ch;
        ch.name = "Synth 1";
        ch.waveform = Waveform::Saw;
        state.project.channels.push_back(ch);
    }
    if (state.project.patterns.empty()) {
        Pattern pat;
        pat.name = "Pattern 1";
        pat.channelIndex = 0;
        for (int i = 0; i < 8; i++) {
            Note n;
            n.start = i * 480;
            n.length = 240;
            n.key = 60 + (i % 4) * 2;
            n.velocity = 100;
            pat.notes.push_back(n);
        }
        state.project.patterns.push_back(pat);
        state.project.selectedPattern = 0;
    }
    state.browserPath = ".";
    refreshBrowser(state, ".");
    if (engine) engine->setMasterVolume(state.masterVolume);
    return true;
}

void appShutdown(AppState& state) {
    if (state.engine) {
        state.engine->stopPlayback();
    }
}

void refreshBrowser(AppState& state, const char* path) {
    state.browserFiles.clear();
    state.browserIsDir.clear();
    state.browserSelected = -1;
    state.browserPath = path;
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "opendir failed: %s\n", path);
        return;
    }
    std::string base = path;
    if (base == ".") {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        base = cwd;
    }
    if (!base.empty() && base.back() == '/') base.pop_back();
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string fullPath = base + "/" + name;
        struct stat st;
        bool isDir = stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        state.browserFiles.push_back(name);
        state.browserIsDir.push_back(isDir);
    }
    closedir(dir);
    std::vector<int> idx(state.browserFiles.size());
    for (int i = 0; i < (int)idx.size(); i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        if (state.browserIsDir[a] != state.browserIsDir[b])
            return state.browserIsDir[a] > state.browserIsDir[b];
        std::string la, lb;
        for (char c : state.browserFiles[a]) la += (char)std::tolower((unsigned char)c);
        for (char c : state.browserFiles[b]) lb += (char)std::tolower((unsigned char)c);
        return la < lb;
    });
    std::vector<std::string> sortedFiles;
    std::vector<bool> sortedIsDir;
    for (int i : idx) {
        sortedFiles.push_back(state.browserFiles[i]);
        sortedIsDir.push_back(state.browserIsDir[i]);
    }
    state.browserFiles = std::move(sortedFiles);
    state.browserIsDir = std::move(sortedIsDir);
}

void addChannel(AppState& state, bool isSampler) {
    pushUndo(state);
    Channel ch;
    ch.name = isSampler ? "Sampler " + std::to_string(state.project.channels.size() + 1)
                        : "Synth " + std::to_string(state.project.channels.size() + 1);
    ch.isSampler = isSampler;
    ch.mixerChannel = (int)state.project.channels.size() % NUM_MIXER_SLOTS;
    state.project.channels.push_back(ch);
    state.project.selectedChannel = (int)state.project.channels.size() - 1;
    state.project.modified = true;
}

void addSF2Channel(AppState& state) {
    pushUndo(state);
    Channel ch;
    ch.name = "SF2 " + std::to_string(state.project.channels.size() + 1);
    ch.useSF2 = true;
    ch.mixerChannel = (int)state.project.channels.size() % NUM_MIXER_SLOTS;
    state.project.channels.push_back(ch);
    state.project.selectedChannel = (int)state.project.channels.size() - 1;
    state.project.modified = true;
}

void addPattern(AppState& state) {
    pushUndo(state);
    int maxNum = 0;
    for (auto& p : state.project.patterns) {
        if (p.name.size() > 8 && p.name.substr(0, 8) == "Pattern ") {
            int num = std::atoi(p.name.c_str() + 8);
            if (num > maxNum) maxNum = num;
        }
    }
    Pattern pat;
    pat.name = "Pattern " + std::to_string(maxNum + 1);
    pat.channelIndex = state.project.selectedChannel;
    pat.color = IM_COL32(rand() % 200 + 55, rand() % 200 + 55, rand() % 200 + 55, 255);
    state.project.patterns.push_back(pat);
    state.project.selectedPattern = (int)state.project.patterns.size() - 1;
    state.project.modified = true;
}

bool appSaveProject(AppState& state, const char* path) {
    bool ok = ProjectFile::save(state.project, path);
    if (ok) {
        state.project.filePath = path;
        state.project.modified = false;
        setStatus(state, "Saved project to %s", path);
    } else {
        setStatus(state, "Failed to save project!");
    }
    return ok;
}

bool appLoadProject(AppState& state, const char* path) {
    Project p;
    bool ok = ProjectFile::load(p, path);
    if (ok) {
        state.project = std::move(p);
        state.project.ensureMixerSlots();
        state.engine->stopPlayback();
        state.undoStack.clear();
        state.redoStack.clear();
        setStatus(state, "Loaded project: %s", path);
    } else {
        setStatus(state, "Failed to load project!");
    }
    return ok;
}

bool appExportWAV(AppState& state, const char* path) {
    if (!state.engine) { setStatus(state, "Export failed: no engine!"); return false; }
    bool ok = state.engine->renderToWav(state.project, state.transport, path, state.exportSampleRate);
    if (ok) setStatus(state, "Exported to %s", path);
    else setStatus(state, "Export failed!");
    return ok;
}

bool appImportMIDI(AppState& state, const char* path) {
    bool ok = projectImportMIDI(state.project, path);
    if (ok) setStatus(state, "Imported MIDI: %s", path);
    else setStatus(state, "MIDI import failed!");
    return ok;
}

bool appExportMIDI(AppState& state, const char* path) {
    bool ok = projectExportMIDI(state.project, path);
    if (ok) setStatus(state, "Exported MIDI: %s", path);
    else setStatus(state, "MIDI export failed!");
    return ok;
}

void renderMainMenu(AppState& state) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                if (state.project.modified)
                    state.pendingAction = 1;
                else {
                    state.project = Project();
                    state.project.ensureMixerSlots();
                    addChannel(state, false);
                    addPattern(state);
                    state.engine->stopPlayback();
                }
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                if (state.project.modified)
                    state.pendingAction = 3;
                else
                    state.actionOpen = true;
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!state.project.filePath.empty())
                    appSaveProject(state, state.project.filePath.c_str());
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                state.actionSaveAs = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import MIDI...")) {
                state.actionImportMIDI = true;
            }
            if (ImGui::MenuItem("Export WAV...")) {
                state.showExportDialog = true;
            }
            if (ImGui::MenuItem("Export MIDI...")) {
                FILE* fp = popen("zenity --file-selection --save --confirm-overwrite --title='Export MIDI' --file-filter='*.mid' 2>/dev/null", "r");
                if (fp) {
                    char mpath[4096];
                    if (fgets(mpath, sizeof(mpath), fp)) {
                        size_t len = strlen(mpath);
                        if (len > 0 && mpath[len-1] == '\n') mpath[len-1] = '\0';
                        if (strlen(mpath) > 0) {
                            if (strcmp(mpath + strlen(mpath) - 4, ".mid") != 0) strncat(mpath, ".mid", sizeof(mpath) - strlen(mpath) - 1);
                            appExportMIDI(state, mpath);
                        }
                    }
                    pclose(fp);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
                if (state.project.modified)
                    state.pendingAction = 2;
                else
                    state.requestQuit = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !state.undoStack.empty())) {
                undo(state);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, !state.redoStack.empty())) {
                redo(state);
            }
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X", false, false);
            ImGui::MenuItem("Copy", "Ctrl+C", false, false);
            ImGui::MenuItem("Paste", "Ctrl+V", false, false);
            ImGui::MenuItem("Delete", "Del", false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Channels")) {
            if (ImGui::MenuItem("Add Synth Channel")) { addChannel(state, false); }
            if (ImGui::MenuItem("Add Sampler Channel")) { addChannel(state, true); }
            if (ImGui::MenuItem("Add SF2 Channel")) { addSF2Channel(state); }
            ImGui::Separator();
            if (ImGui::MenuItem("Add Pattern")) { addPattern(state); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Playlist", nullptr, &state.showPlaylist);
            ImGui::MenuItem("Piano Roll", nullptr, &state.showPianoRoll);
            ImGui::MenuItem("Channel Rack", nullptr, &state.showChannelRack);
            ImGui::MenuItem("Mixer", nullptr, &state.showMixer);
            ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About neoDAW")) { state.showAboutDialog = true; }
            ImGui::EndMenu();
        }

        float bpm = state.project.bpm;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 180);
        ImGui::Text("BPM: "); ImGui::SameLine();
        ImGui::PushItemWidth(80);
        ImGui::DragFloat("##bpm", &bpm, 0.5f, 20.0f, 300.0f, "%.1f");
        ImGui::PopItemWidth();
        if (bpm != state.project.bpm) {
            pushUndo(state);
            state.project.bpm = bpm;
            state.engine->setBPM(bpm);
            state.project.modified = true;
        }
        ImGui::SetItemTooltip("Beats per minute");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,0.67f,0,1), "neoDAW");
        ImGui::EndMainMenuBar();
    }
}

void renderTransportBar(AppState& state) {
    bool playing = state.transport.state.load() == TransportState::Playing;
    bool paused = state.transport.state.load() == TransportState::Paused;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImVec2 menuEnd(0, ImGui::GetFrameHeight());
    ImGui::SetNextWindowPos(menuEnd, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, 0));
    ImGui::Begin("Transport", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    if (ImGui::ArrowButton("prev", ImGuiDir_Left)) {
        int cur = state.project.selectedPattern;
        if (cur > 0) state.project.selectedPattern = cur - 1;
    }
    ImGui::SetItemTooltip("Previous pattern");
    ImGui::SameLine();
    if (ImGui::Button(playing ? "||" : ">", ImVec2(40, 24))) {
        if (playing || paused) state.engine->pausePlayback(); else state.engine->startPlayback();
    }
    ImGui::SetItemTooltip("Play / Pause");
    ImGui::SameLine();
    if (ImGui::Button("[]", ImVec2(40, 24))) {
        state.engine->stopPlayback();
    }
    ImGui::SetItemTooltip("Stop");
    ImGui::SameLine();
    bool loop = state.transport.looping.load();
    if (ImGui::Button(loop ? "Loop" : "Once", ImVec2(50, 24))) {
        state.transport.looping.store(!loop);
    }
    ImGui::SetItemTooltip("Toggle loop");
    ImGui::SameLine();
    int ppq = state.project.ppq;
    int playHead = state.transport.playHead.load();
    int bars = playHead / (ppq * state.project.beatsPerBar);
    int beats = (playHead % (ppq * state.project.beatsPerBar)) / ppq;
    int ticks = playHead % ppq;
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%d.%d.%03d", bars + 1, beats + 1, ticks);
    ImGui::Text("%s", timeStr);
    ImGui::SameLine();
    ImGui::Text("  Pattern: "); ImGui::SameLine();
    if (state.project.selectedPattern >= 0 &&
        state.project.selectedPattern < (int)state.project.patterns.size()) {
        ImGui::PushItemWidth(120);
        std::string curPat = state.project.patterns[state.project.selectedPattern].name;
        if (ImGui::BeginCombo("##patSel", curPat.c_str())) {
            for (int i = 0; i < (int)state.project.patterns.size(); i++) {
                bool isSelected = (i == state.project.selectedPattern);
                if (ImGui::Selectable(state.project.patterns[i].name.c_str(), isSelected)) {
                    state.project.selectedPattern = i;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200,80,80,255));
        if (ImGui::SmallButton("X##delPat")) {
            pushUndo(state);
            state.project.patterns.erase(state.project.patterns.begin() + state.project.selectedPattern);
            if (state.project.patterns.empty()) addPattern(state);
            state.project.selectedPattern = std::max(0, state.project.selectedPattern - 1);
            state.project.modified = true;
        }
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Delete pattern");
    }
    ImGui::SetItemTooltip("Select pattern");

    if (state.statusTimer > 0 && state.statusMessage[0]) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,1,0.6f,1), "%s", state.statusMessage);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void renderChannelRack(AppState& state) {
    ImGui::Begin("Channel Rack", &state.showChannelRack, ImGuiWindowFlags_NoFocusOnAppearing);
    if (ImGui::Button("+ Synth", ImVec2(70, 22))) addChannel(state, false);
    ImGui::SetItemTooltip("Add a synth channel");
    ImGui::SameLine();
    if (ImGui::Button("+ Sampler", ImVec2(75, 22))) addChannel(state, true);
    ImGui::SetItemTooltip("Add a sampler channel");
    ImGui::SameLine();
    if (ImGui::Button("+ SF2", ImVec2(50, 22))) addSF2Channel(state);
    ImGui::SetItemTooltip("Add an SF2 soundfont channel");
    ImGui::SameLine();
    if (ImGui::Button("+ Pattern", ImVec2(75, 22))) addPattern(state);
    ImGui::SetItemTooltip("Add a new pattern");
    auto& channels = state.project.channels;
    if (channels.empty()) {
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "No channels. Click + Synth to add one.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    float totalHeight = 0;
    int chanToRemove = -1;
    for (int c = 0; c < (int)channels.size(); c++) {
        auto& ch = channels[c];
        ImGui::PushID(c);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f,0.18f,0.20f,0.8f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f,0.22f,0.25f,0.8f));
        bool selected = (c == state.project.selectedChannel);
        if (ImGui::Selectable("##ch", selected, ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            state.project.selectedChannel = c;
        }
        ImGui::PopStyleColor(2);
        bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDoubleClicked(0)) {
            state.renameChannelIndex = c;
        }

        float lineH = ImGui::GetTextLineHeight();
        ImVec2 pos = ImGui::GetItemRectMin();
        const char* icon = ch.useSF2 ? "F" : (ch.isSampler ? "S" : "~");
        ImU32 iconCol = ch.useSF2 ? IM_COL32(180,255,100,255) : (ch.isSampler ? IM_COL32(100,200,255,255) : IM_COL32(255,200,100,255));
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(pos.x + 4, pos.y + 1),
            iconCol, icon);

        ImGui::SameLine(); ImGui::SetCursorPosX(pos.x + 20);
        ImGui::Text("%s", ch.name.c_str());
        ImGui::SameLine(); ImGui::SetCursorPosX(pos.x + ImGui::GetContentRegionAvail().x - 130);
        ImGui::PushItemWidth(40);
        ImGui::PushStyleColor(ImGuiCol_Text, ch.muted ? COL_MUTED : IM_COL32(180,180,180,255));
        if (ImGui::SmallButton("M")) {
            pushUndo(state);
            ch.muted = !ch.muted;
            state.project.modified = true;
        }
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Mute / Unmute");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ch.soloed ? COL_SOLOED : IM_COL32(180,180,180,255));
        if (ImGui::SmallButton("S")) {
            pushUndo(state);
            ch.soloed = !ch.soloed;
            state.project.modified = true;
        }
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Solo / Unsolo");
        ImGui::SameLine();
        float vol = ch.volume;
        ImGui::PushItemWidth(50);
        if (ImGui::DragFloat("##vol", &vol, 0.01f, 0.0f, 1.5f, "%.2f")) {
            ch.volume = std::clamp(vol, 0.0f, 1.5f);
            state.project.modified = true;
        }
        ImGui::SetItemTooltip("Channel volume");
        ImGui::PopItemWidth();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200,80,80,255));
        if (ImGui::SmallButton("X")) {
            chanToRemove = c;
        }
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Delete channel");
        totalHeight += lineH + 4;
        ImGui::PopID();
    }

    if (chanToRemove >= 0) {
        pushUndo(state);
        int ci = chanToRemove;
        channels.erase(channels.begin() + ci);
        if (state.project.selectedChannel >= (int)channels.size())
            state.project.selectedChannel = std::max(0, (int)channels.size() - 1);
        state.project.modified = true;
    }

    if (state.renameChannelIndex >= 0 && state.renameChannelIndex < (int)channels.size()) {
        ImGui::OpenPopup("Rename Channel");
        if (ImGui::BeginPopupModal("Rename Channel", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char renameBuf[64];
            static bool initBuf = false;
            if (!initBuf) {
                strncpy(renameBuf, channels[state.renameChannelIndex].name.c_str(), sizeof(renameBuf));
                renameBuf[sizeof(renameBuf)-1] = '\0';
                initBuf = true;
            }
            ImGui::Text("Channel: %s", channels[state.renameChannelIndex].name.c_str());
            ImGui::InputText("Name", renameBuf, sizeof(renameBuf));
            if (ImGui::Button("OK", ImVec2(60, 0))) {
                pushUndo(state);
                channels[state.renameChannelIndex].name = renameBuf;
                state.project.modified = true;
                state.renameChannelIndex = -1;
                initBuf = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(60, 0))) {
                state.renameChannelIndex = -1;
                initBuf = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    ImGui::Text("Samples / Presets");
    ImGui::Separator();
    int selCh = state.project.selectedChannel;
    if (selCh >= 0 && selCh < (int)state.project.channels.size()) {
        auto& ch = state.project.channels[selCh];
        if (ch.useSF2) {
            ImGui::Text("SF2: %s", ch.sf2Path.empty() ? "(none)" : ch.sf2Path.c_str());
            if (!ch.sf2Path.empty()) {
                ImGui::PushItemWidth(60);
                int preset = ch.sf2Preset;
                if (ImGui::DragInt("Preset", &preset, 1, 0, 127)) {
                    ch.sf2Preset = std::clamp(preset, 0, 127);
                    state.project.modified = true;
                    if (state.engine && state.engine->fluidSfontId >= 0) {
                        state.engine->lockAudio();
                        if (!state.engine->selectSFontPreset(state.engine->fluidSfontId, 0, ch.sf2Preset, selCh)) {
                            for (int p = 0; p < 128; p++) {
                                if (state.engine->selectSFontPreset(state.engine->fluidSfontId, 0, p, selCh)) {
                                    ch.sf2Preset = p; break;
                                }
                            }
                        }
                        state.engine->unlockAudio();
                    }
                }
                ImGui::PopItemWidth();
            }
        }
    }
    ImGui::End();
}

void renderPianoRoll(AppState& state) {
    ImGui::Begin("Piano Roll", &state.showPianoRoll, ImGuiWindowFlags_NoFocusOnAppearing);
    if (state.project.selectedPattern < 0 ||
        state.project.selectedPattern >= (int)state.project.patterns.size()) {
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "No pattern selected.");
        ImGui::End();
        return;
    }

    auto& pattern = state.project.patterns[state.project.selectedPattern];
    int ppq = state.project.ppq;
    int beatsPerBar = state.project.beatsPerBar;
    int totalTicks = pattern.lengthTicks;
    int numBars = (totalTicks + ppq * beatsPerBar - 1) / (ppq * beatsPerBar);
    ImGui::PushItemWidth(60);
    ImGui::LabelText("##patName", "%s", pattern.name.c_str());
    ImGui::SameLine();
    int lenBars = totalTicks / (ppq * beatsPerBar);
    if (ImGui::DragInt("Bars", &lenBars, 0.5f, 1, 64)) {
        pushUndo(state);
        pattern.lengthTicks = lenBars * ppq * beatsPerBar;
        state.project.modified = true;
    }
    ImGui::SetItemTooltip("Pattern length in bars");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("Zoom", &state.pianoRollZoomX, 0.5f, 4.0f, 64.0f);
    ImGui::SetItemTooltip("Horizontal zoom");
    ImGui::SameLine();
    const char* snapLabels[] = {"1", "1/2", "1/4", "1/8", "1/16", "1/32"};
    int snapValues[] = {1, 2, 4, 8, 16, 32};
    int snapIdx = 0;
    for (int i = 0; i < 6; i++) {
        if (state.pianoRollSnap == snapValues[i]) { snapIdx = i; break; }
    }
    ImGui::PushItemWidth(60);
    if (ImGui::BeginCombo("##snap", snapLabels[snapIdx])) {
        for (int i = 0; i < 6; i++) {
            bool sel = (snapIdx == i);
            if (ImGui::Selectable(snapLabels[i], sel)) {
                state.pianoRollSnap = snapValues[i];
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Snap resolution");
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &state.pianoRollSnapEnabled);
    ImGui::SetItemTooltip("Toggle snap to grid");
    ImGui::PopItemWidth();
    ImGui::PopItemWidth();
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    float keyWidth = 50.0f;
    float rowHeight = 18.0f * state.pianoRollZoomY;
    int numKeys = 48;
    int startKey = 36;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::BeginChild("##pianoRollCanvas", canvasSize, true, ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 childPos = ImGui::GetCursorScreenPos();
    ImVec2 childSize = ImGui::GetContentRegionAvail();
    float canvasW = std::max(childSize.x, keyWidth + totalTicks * state.pianoRollZoomX / (float)ppq + 100);
    float canvasH = std::max(childSize.y, numKeys * rowHeight + 20);
    dl->AddRectFilled(childPos, ImVec2(childPos.x + canvasW, childPos.y + canvasH), COL_BG);
    float zoomX = state.pianoRollZoomX;

    int snapTicks = ppq / state.pianoRollSnap;
    if (state.pianoRollSnapEnabled && snapTicks > 0) {
        for (int t = 0; t <= totalTicks; t += snapTicks) {
            float x = childPos.x + keyWidth + t * zoomX / (float)ppq;
            bool isBeat = (t % ppq == 0);
            bool isBar = (t % (ppq * beatsPerBar) == 0);
            if (!isBeat && !isBar)dl->AddLine(ImVec2(x, childPos.y), ImVec2(x, childPos.y + canvasH), COL_GRID_SNAP, 0.5f);
        }
    }
    for (int t = 0; t <= totalTicks; t += ppq / 4) {
        float x = childPos.x + keyWidth + t * zoomX / (float)ppq;
        bool isBeat = (t % ppq == 0);
        bool isBar = (t % (ppq * beatsPerBar) == 0);
        dl->AddLine(ImVec2(x, childPos.y), ImVec2(x, childPos.y + canvasH), isBar ? COL_GRID_BEAT : COL_GRID_LINE, isBar ? 1.5f : 0.5f);
    }

    for (int k = 0; k < numKeys; k++) {
        int key = startKey + (numKeys - 1 - k);
        float y = childPos.y + k * rowHeight;
        bool isBlack = false;
        int noteInOct = key % 12;
        isBlack = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);
        dl->AddRectFilled(ImVec2(childPos.x, y),
                          ImVec2(childPos.x + keyWidth, y + rowHeight),
                          isBlack ? COL_KEY_BLACK : COL_KEY_WHITE);
        if (!isBlack) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%s%d", keyToNoteName(key), key / 12 - 1);
            dl->AddText(ImVec2(childPos.x + 4, y + 1),
                        isBlack ? IM_COL32(200,200,200,255) : IM_COL32(30,30,30,255), buf);
        }
    }

    float endY = childPos.y + numKeys * rowHeight;
    dl->AddLine(ImVec2(childPos.x + keyWidth, childPos.y),
                ImVec2(childPos.x + keyWidth, endY), IM_COL32(100,100,100,255), 2.0f);

    for (int ni = 0; ni < (int)pattern.notes.size(); ni++) {
        auto& note = pattern.notes[ni];
        float x = childPos.x + keyWidth + note.start * zoomX / (float)ppq;
        float w = std::max(2.0f, note.length * zoomX / (float)ppq);
        int keyIdx = numKeys - 1 - (note.key - startKey);
        if (keyIdx < 0 || keyIdx >= numKeys) continue;
        float y = childPos.y + keyIdx * rowHeight;
        bool active = (ni == state.pianoRollEditingNote);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + rowHeight),
                          active ? COL_NOTE_ACTIVE : COL_NOTE, 2.0f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + rowHeight),
                    IM_COL32(255,255,255,80), 2.0f);

        char buf[8];
        snprintf(buf, sizeof(buf), "%s%d", keyToNoteName(note.key), note.key / 12 - 1);
        dl->AddText(ImVec2(x + 3, y + 1), IM_COL32(255,255,255,200), buf);
    }

    int playHead = state.transport.playHead.load();
    float phx = childPos.x + keyWidth + playHead * zoomX / (float)ppq;
    dl->AddLine(ImVec2(phx, childPos.y), ImVec2(phx, childPos.y + canvasH),
                IM_COL32(255,50,50,200), 2.0f);

    ImGui::InvisibleButton("##pianoRollClick", ImVec2(canvasW, canvasH));
    bool isHovered = ImGui::IsItemHovered();
    if (isHovered) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float relX = mousePos.x - childPos.x - keyWidth;
        float relY = mousePos.y - childPos.y;
        if (relX >= 0 && relY >= 0) {
            int tick = (int)(relX * ppq / zoomX);
            int keyIdx = (int)(relY / rowHeight);
            int key = startKey + (numKeys - 1 - keyIdx);
            if (tick >= 0 && tick < totalTicks && key >= 0 && key < 128) {
                bool ctrl = ImGui::GetIO().KeyCtrl;
                bool shift = ImGui::GetIO().KeyShift;
                if (ImGui::IsMouseClicked(0) && !state.pianoRollDragging && !state.pianoRollResizing) {
                    if (ctrl) {
                        state.transport.playHead.store(tick);
                        if (state.engine) state.engine->stopPlayback();
                    } else {
                        bool hit = false;
                        for (int ni = 0; ni < (int)pattern.notes.size(); ni++) {
                            auto& n = pattern.notes[ni];
                            float nx = childPos.x + keyWidth + n.start * zoomX / (float)ppq;
                            float nw = std::max(4.0f, n.length * zoomX / (float)ppq);
                            int nKeyIdx = numKeys - 1 - (n.key - startKey);
                            float ny = childPos.y + nKeyIdx * rowHeight;
                            if (mousePos.x >= nx && mousePos.x <= nx + nw &&
                                mousePos.y >= ny && mousePos.y <= ny + rowHeight) {
                                state.pianoRollEditingNote = ni;
                                if (state.engine) {
                                    if (state.pianoRollPreviewKey >= 0) state.engine->previewNoteOff(state.pianoRollPreviewKey);
                                    state.engine->previewChannel = state.project.selectedChannel;
                                    state.engine->previewNoteOn(n.key);
                                    state.pianoRollPreviewKey = n.key;
                                }
                                if (mousePos.x >= nx + nw - 6) {
                                    state.pianoRollResizing = true;
                                    state.pianoRollResizingNote = ni;
                                    state.pianoRollDragStartX = mousePos.x;
                                    pushUndo(state);
                                    state.pianoRollDragPushedUndo = true;
                                } else {
                                    state.pianoRollDragging = true;
                                    state.pianoRollDragNote = ni;
                                    state.pianoRollDragStartX = mousePos.x;
                                    state.pianoRollDragStartY = mousePos.y;
                                    state.pianoRollDragStartKey = n.key;
                                    state.pianoRollDragStartTick = n.start;
                                    pushUndo(state);
                                    state.pianoRollDragPushedUndo = true;
                                }
                                hit = true;
                                break;
                            }
                        }
                        if (!hit) {
                            if (shift) {
                                Note newNote;
                                int snapT = state.pianoRollSnapEnabled ? ppq / state.pianoRollSnap : 1;
                                newNote.start = (tick / snapT) * snapT;
                                newNote.length = ppq / 2;
                                newNote.key = key;
                                newNote.velocity = 100;
                                pushUndo(state);
                                pattern.notes.push_back(newNote);
                                state.project.modified = true;
                                state.pianoRollEditingNote = (int)pattern.notes.size() - 1;
                            } else {
                                state.transport.playHead.store(tick);
                                if (state.engine) state.engine->stopPlayback();
                            }
                        }
                    }
                }

                if (ImGui::IsMouseDragging(0) && state.pianoRollDragNote >= 0 &&
                    state.pianoRollDragNote < (int)pattern.notes.size()) {
                    auto& dragNote = pattern.notes[state.pianoRollDragNote];
                    int deltaTick = (int)((ImGui::GetMousePos().x - state.pianoRollDragStartX) * ppq / zoomX);
                    int newStart = std::max(0, state.pianoRollDragStartTick + deltaTick);
                    if (state.pianoRollSnapEnabled) {
                        int snapT = ppq / state.pianoRollSnap;
                        newStart = (newStart / snapT) * snapT;
                    }
                    dragNote.start = newStart;
                    float deltaY = ImGui::GetMousePos().y - state.pianoRollDragStartY;
                    int deltaKey = -(int)std::round(deltaY / rowHeight);
                    dragNote.key = std::clamp(state.pianoRollDragStartKey + deltaKey, 0, 127);
                    state.project.modified = true;
                }

                if (ImGui::IsMouseDragging(0) && state.pianoRollResizingNote >= 0 &&
                    state.pianoRollResizingNote < (int)pattern.notes.size()) {
                    auto& resizeNote = pattern.notes[state.pianoRollResizingNote];
                    int deltaTick = (int)((ImGui::GetMousePos().x - state.pianoRollDragStartX) * ppq / zoomX);
                    resizeNote.length = std::max(ppq/8, resizeNote.length + deltaTick);
                    state.pianoRollDragStartX = ImGui::GetMousePos().x;
                    state.project.modified = true;
                }

                if (ctrl && ImGui::IsMouseDown(0) && !shift && !ImGui::IsMouseClicked(0) &&
                    !state.pianoRollDragging && !state.pianoRollResizing) {
                    state.transport.playHead.store(tick);
                    if (state.engine) state.engine->stopPlayback();
                }
            }
        }

        float mouseX = ImGui::GetMousePos().x;
        float mouseY = ImGui::GetMousePos().y;
        if (mouseX >= childPos.x && mouseX < childPos.x + keyWidth &&
            mouseY >= childPos.y && mouseY < childPos.y + numKeys * rowHeight) {
            int keyIdx = (int)((mouseY - childPos.y) / rowHeight);
            int key = startKey + (numKeys - 1 - keyIdx);
            if (key >= 0 && key < 128 && state.engine) {
                if (ImGui::IsMouseClicked(0)) {
                    state.engine->previewChannel = state.project.selectedChannel;
                    state.engine->previewNoteOn(key);
                    state.pianoRollPreviewKey = key;
                } else if (ImGui::IsMouseReleased(0) && state.pianoRollPreviewKey >= 0) {
                    state.engine->previewNoteOff(state.pianoRollPreviewKey);
                    state.pianoRollPreviewKey = -1;
                }
            }
        } else if (ImGui::IsMouseReleased(0) && state.pianoRollPreviewKey >= 0) {
            state.engine->previewNoteOff(state.pianoRollPreviewKey);
            state.pianoRollPreviewKey = -1;
        }

        if (ImGui::IsMouseClicked(1)) {
            float relX = ImGui::GetMousePos().x - childPos.x - keyWidth;
            float relY = ImGui::GetMousePos().y - childPos.y;
            if (relX >= 0 && relY >= 0) {
                int tick = (int)(relX * ppq / zoomX);
                int keyIdx = (int)(relY / rowHeight);
                int key = startKey + (numKeys - 1 - keyIdx);
                for (int ni = 0; ni < (int)pattern.notes.size(); ni++) {
                    auto& n = pattern.notes[ni];
                    float nx = childPos.x + keyWidth + n.start * zoomX / (float)ppq;
                    float nw = std::max(2.0f, n.length * zoomX / (float)ppq);
                    int nKeyIdx = numKeys - 1 - (n.key - startKey);
                    float ny = childPos.y + nKeyIdx * rowHeight;
                    if (ImGui::GetMousePos().x >= nx && ImGui::GetMousePos().x <= nx + nw &&
                        ImGui::GetMousePos().y >= ny && ImGui::GetMousePos().y <= ny + rowHeight) {
                        pushUndo(state);
                        pattern.notes.erase(pattern.notes.begin() + ni);
                        state.project.modified = true;
                        state.pianoRollEditingNote = -1;
                        break;
                    }
                }
            }
        }
    }

    if (ImGui::IsMouseClicked(2) && ImGui::IsItemHovered()) {
        state.pianoRollScrolling = true;
        state.pianoRollScrollStartX = (int)ImGui::GetMousePos().x;
        state.pianoRollScrollStartY = (int)ImGui::GetMousePos().y;
        state.pianoRollScrollOrigX = (int)ImGui::GetScrollX();
        state.pianoRollScrollOrigY = (int)ImGui::GetScrollY();
    }
    if (state.pianoRollScrolling) {
        if (ImGui::IsMouseDown(2)) {
            float dx = ImGui::GetMousePos().x - state.pianoRollScrollStartX;
            float dy = ImGui::GetMousePos().y - state.pianoRollScrollStartY;
            ImGui::SetScrollX(std::max(0.0f, state.pianoRollScrollOrigX - dx));
            ImGui::SetScrollY(std::max(0.0f, state.pianoRollScrollOrigY - dy));
        } else {
            state.pianoRollScrolling = false;
        }
    }

    if (!ImGui::IsMouseDown(0)) {
        state.pianoRollDragging = false;
        state.pianoRollDragNote = -1;
        state.pianoRollResizing = false;
        state.pianoRollResizingNote = -1;
        state.pianoRollDragPushedUndo = false;
        if (state.pianoRollPreviewKey >= 0 && state.engine) {
            state.engine->previewNoteOff(state.pianoRollPreviewKey);
            state.pianoRollPreviewKey = -1;
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void renderPlaylist(AppState& state) {
    ImGui::Begin("Playlist", &state.showPlaylist, ImGuiWindowFlags_NoFocusOnAppearing);
    if (ImGui::Button("+ Clip", ImVec2(60, 22))) {
        if (!state.project.channels.empty()) {
            if (state.project.patterns.empty()) addPattern(state);
            pushUndo(state);
            PlaylistClip clip;
            clip.track = 0;
            clip.startTick = 0;
            clip.patternIndex = state.project.selectedPattern >= 0 ? state.project.selectedPattern : 0;
            state.project.playlist.push_back(clip);
            state.project.modified = true;
        }
    }
    ImGui::SetItemTooltip("Add a clip referencing the current pattern");
    ImGui::SameLine();
    if (ImGui::Button("+ Pattern", ImVec2(75, 22))) {
        addPattern(state);
    }
    ImGui::SetItemTooltip("Create a new pattern");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear All")) {
        pushUndo(state);
        state.project.playlist.clear();
        state.project.modified = true;
    }
    ImGui::SetItemTooltip("Remove all clips");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("Zoom", &state.playlistZoomX, 0.5f, 2.0f, 64.0f);
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    int ppq = state.project.ppq;
    int beatsPerBar = state.project.beatsPerBar;
    float zoomX = state.playlistZoomX;
    int totalTicks = ppq * beatsPerBar * 64;
    for (auto& clip : state.project.playlist) {
        if (clip.patternIndex >= 0 && clip.patternIndex < (int)state.project.patterns.size()) {
            int end = clip.startTick + state.project.patterns[clip.patternIndex].lengthTicks;
            if (end > totalTicks) totalTicks = end;
        }
    }
    totalTicks = std::max(totalTicks, ppq * beatsPerBar * 4);
    float trackHeight = 40.0f;
    int numTracks = std::max(1, (int)state.project.channels.size());
    ImGui::BeginChild("##playlistCanvas", canvasSize, true, ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 childPos = ImGui::GetCursorScreenPos();
    float canvasW = std::max(canvasSize.x, 100.0f + totalTicks * zoomX / (float)ppq);
    float canvasH = std::max(canvasSize.y, (numTracks + 1) * trackHeight + 30);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(childPos, ImVec2(childPos.x + canvasW, childPos.y + canvasH), COL_BG);
    float headerW = 120.0f;
    for (int t = 0; t < numTracks; t++) {
        float y = childPos.y + t * trackHeight + 20;
        dl->AddRectFilled(ImVec2(childPos.x, y),
                          ImVec2(childPos.x + headerW, y + trackHeight), COL_BG2);
        if (t < (int)state.project.channels.size()) {
            dl->AddText(ImVec2(childPos.x + 4, y + 2), COL_TEXT,
                        state.project.channels[t].name.c_str());
        }
    }

    for (int t = 0; t <= totalTicks; t += ppq / 4) {
        float x = childPos.x + headerW + t * zoomX / (float)ppq;
        bool isBeat = (t % ppq == 0);
        bool isBar = (t % (ppq * beatsPerBar) == 0);
        dl->AddLine(ImVec2(x, childPos.y + 20), ImVec2(x, childPos.y + canvasH),
                    isBar ? COL_GRID_BEAT : COL_GRID_LINE, isBar ? 1.5f : 0.5f);
    }

    int clipToRemove = -1;
    bool hoveredClip = false;
    for (int ci = 0; ci < (int)state.project.playlist.size(); ci++) {
        auto& clip = state.project.playlist[ci];
        if (clip.patternIndex < 0 || clip.patternIndex >= (int)state.project.patterns.size()) continue;
        auto& pat = state.project.patterns[clip.patternIndex];
        int track = clip.track;
        if (track < 0 || track >= numTracks) continue;
        float x = childPos.x + headerW + clip.startTick * zoomX / (float)ppq;
        float y = childPos.y + track * trackHeight + 20;
        float w = std::max(4.0f, pat.lengthTicks * zoomX / (float)ppq);
        ImU32 col = clip.muted ? IM_COL32(60,60,60,150) : COL_PLAYLIST_CLIP;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + trackHeight), col, 3.0f);
        int patChanIdx = pat.channelIndex;
        if (patChanIdx >= 0 && patChanIdx < (int)state.project.channels.size()) {
            auto& ch = state.project.channels[patChanIdx];
            if (!ch.isSampler && !pat.notes.empty()) {
                int minKey = pat.notes[0].key, maxKey = pat.notes[0].key;
                for (auto& n : pat.notes) {
                    minKey = std::min(minKey, n.key);
                    maxKey = std::max(maxKey, n.key);
                }
                int keyRange = std::max(12, maxKey - minKey);
                int r = (pat.color >> 16) & 0xFF;
                int gr = (pat.color >> 8) & 0xFF;
                int bl = (pat.color >> 0) & 0xFF;
                float noteH = std::max(4.0f, (trackHeight - 6) / (keyRange / 4.0f));
                noteH = std::min(noteH, 10.0f);
                for (auto& n : pat.notes) {
                    float nx = x + (n.start / (float)pat.lengthTicks) * w;
                    float nw = std::max(2.0f, (n.length / (float)pat.lengthTicks) * w);
                    float keyFrac = (maxKey - n.key) / (float)keyRange;
                    float ny = y + 3 + keyFrac * (trackHeight - 6 - noteH);
                    dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + nw, ny + noteH),
                                      IM_COL32(r, gr, bl, 180), 1.0f);
                    dl->AddRect(ImVec2(nx, ny), ImVec2(nx + nw, ny + noteH),
                                IM_COL32(255,255,255,40), 1.0f);
                }
            } else if (ch.isSampler && ch.sampleIndex >= 0 && state.engine) {
                auto* sd = state.engine->getSample(ch.sampleIndex);
                if (sd && sd->loaded && !sd->samples.empty()) {
                    int numBars = std::min((int)w, 200);
                    if (numBars > 1) {
                        float centerY = y + trackHeight * 0.5f;
                        float halfH = (trackHeight - 6) * 0.5f;
                        size_t totalSamples = sd->samples.size();
                        for (int b = 0; b < numBars; b++) {
                            float peak = 0.0f;
                            size_t startS = (size_t)((b / (float)numBars) * totalSamples);
                            size_t endS = (size_t)(((b + 1) / (float)numBars) * totalSamples);
                            endS = std::min(endS, totalSamples);
                            for (size_t s = startS; s < endS; s++)
                                peak = std::max(peak, std::abs(sd->samples[s]));
                            float bx = x + (b / (float)numBars) * w;
                            float barH = peak * halfH;
                            if (barH > 0.5f) {
                                dl->AddRectFilled(ImVec2(bx, centerY - barH),
                                                  ImVec2(bx + 2, centerY + barH),
                                                  IM_COL32(100,255,120,180));
                            }
                        }
                    }
                }
            }
        }

        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + trackHeight),
                    IM_COL32(255,255,255,40), 3.0f);

        char buf[64];
        snprintf(buf, sizeof(buf), "%s", pat.name.c_str());
        dl->AddText(ImVec2(x + 4, y + 4), COL_TEXT, buf);
        ImGui::PushID(ci);
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::InvisibleButton("##clip", ImVec2(w, trackHeight));
        if (ImGui::IsItemHovered()) hoveredClip = true;
        if (ImGui::BeginPopupContextItem("##clipCtx")) {
            if (ImGui::MenuItem("Remove")) {
                clipToRemove = ci;
            }
            if (ImGui::MenuItem("Mute/Unmute")) {
                pushUndo(state);
                clip.muted = !clip.muted;
                state.project.modified = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open in Piano Roll")) {
                state.project.selectedPattern = clip.patternIndex;
                state.showPianoRoll = true;
            }
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            state.project.selectedPattern = clip.patternIndex;
            state.showPianoRoll = true;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (!state.playlistDragPushedUndo) {
                pushUndo(state);
                state.playlistDragPushedUndo = true;
                state.playlistDragStartMouseX = ImGui::GetMousePos().x;
                state.playlistDragStartMouseY = ImGui::GetMousePos().y;
                state.playlistDragOrigTick = clip.startTick;
                state.playlistDragOrigTrack = clip.track;
            }
            float dx = ImGui::GetMousePos().x - state.playlistDragStartMouseX;
            float dy = ImGui::GetMousePos().y - state.playlistDragStartMouseY;
            int tickDelta = (int)(dx * ppq / zoomX);
            clip.startTick = std::max(0, state.playlistDragOrigTick + tickDelta / 16 * 16);
            int trackDelta = (int)(dy / trackHeight);
            clip.track = std::clamp(state.playlistDragOrigTrack + trackDelta, 0, numTracks - 1);
            state.project.modified = true;
        }
        ImGui::PopID();
    }

    if (clipToRemove >= 0) {
        pushUndo(state);
        state.project.playlist.erase(state.project.playlist.begin() + clipToRemove);
        state.project.modified = true;
    }

    if (!ImGui::IsMouseDown(0)) {
        state.playlistDragPushedUndo = false;
    }

    ImGui::SetCursorScreenPos(ImVec2(childPos.x + headerW, childPos.y + 20));
    ImGui::InvisibleButton("##gridBg", ImVec2(canvasW - headerW, numTracks * trackHeight + 10));
    if (!hoveredClip && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        int clickTick = (int)((mp.x - childPos.x - headerW) * ppq / zoomX);
        clickTick = (clickTick / ppq) * ppq;
        int clickTrack = (int)((mp.y - childPos.y - 20) / trackHeight);
        if (clickTrack >= 0 && clickTrack < numTracks) {
            if (state.project.patterns.empty()) addPattern(state);
            if (state.project.selectedPattern < 0) state.project.selectedPattern = 0;
            pushUndo(state);
            PlaylistClip nc;
            nc.track = clickTrack;
            nc.startTick = clickTick;
            nc.patternIndex = state.project.selectedPattern;
            state.project.playlist.push_back(nc);
            state.project.modified = true;
        }
    }
    if (!hoveredClip && ImGui::IsItemHovered()) {
        if (ImGui::IsMouseDown(0)) {
            ImVec2 mp = ImGui::GetMousePos();
            int clickTick = (int)((mp.x - childPos.x - headerW) * ppq / zoomX);
            state.transport.playHead.store(clickTick);
            if (state.engine) state.engine->stopPlayback();
        }
    }

    int playHead = state.transport.playHead.load();
    float phx = childPos.x + headerW + playHead * zoomX / (float)ppq;
    dl->AddLine(ImVec2(phx, childPos.y + 20), ImVec2(phx, childPos.y + canvasH),
                IM_COL32(255,50,50,200), 2.0f);

    for (int b = 0; b <= totalTicks / (ppq * beatsPerBar); b++) {
        float x = childPos.x + headerW + b * ppq * beatsPerBar * zoomX / (float)ppq;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", b + 1);
        dl->AddText(ImVec2(x + 2, childPos.y + 2), COL_TEXT_DIM, buf);
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SAMPLE_PATH")) {
            std::string fpath((const char*)payload->Data, payload->DataSize - 1);
            size_t dot = fpath.rfind('.');
            if (dot != std::string::npos) {
                std::string ext;
                for (char c : fpath.substr(dot)) ext += (char)std::tolower((unsigned char)c);
                if (ext == ".mid" || ext == ".midi") {
                    pushUndo(state);
                    int patBefore = (int)state.project.patterns.size();
                    int chBefore = (int)state.project.channels.size();
                    if (appImportMIDI(state, fpath.c_str())) {
                        int patIdx = (int)state.project.patterns.size() - 1;
                        int chIdx = (int)state.project.channels.size() - 1;
                        if (patIdx >= patBefore && chIdx >= chBefore) {
                            state.project.patterns[patIdx].channelIndex = chIdx;
                            PlaylistClip clip;
                            clip.track = chIdx;
                            clip.patternIndex = patIdx;
                            clip.startTick = 0;
                            state.project.playlist.push_back(clip);
                            state.project.modified = true;
                        }
                        setStatus(state, "Imported MIDI: %s", fpath.c_str());
                    }
                } else if (ext == ".sf2") {
                    pushUndo(state);
                    int chIdx = (int)state.project.channels.size();
                    addSF2Channel(state);
                    if (chIdx < (int)state.project.channels.size()) {
                        auto& ch = state.project.channels[chIdx];
                        ch.sf2Path = fpath;
                        if (state.engine) {
                            state.engine->lockAudio();
                            int sfontId = state.engine->loadSFont(fpath);
                            if (sfontId >= 0) {
                                int prog = 0;
                                for (int p = 0; p < 128; p++) {
                                    if (state.engine->selectSFontPreset(sfontId, 0, p, chIdx)) {
                                        prog = p; break;
                                    }
                                }
                                ch.sf2Preset = prog;
                            }
                            state.engine->unlockAudio();
                        }
                        Pattern pat;
                        pat.name = ch.name;
                        pat.lengthTicks = state.project.ppq * 4;
                        pat.channelIndex = chIdx;
                        Note n;
                        n.start = state.project.ppq;
                        n.length = state.project.ppq * 2;
                        n.key = 60;
                        n.velocity = 100;
                        pat.notes.push_back(n);
                        int patIdx = (int)state.project.patterns.size();
                        state.project.patterns.push_back(pat);
                        PlaylistClip clip;
                        clip.track = chIdx;
                        clip.patternIndex = patIdx;
                        clip.startTick = 0;
                        state.project.playlist.push_back(clip);
                        state.project.modified = true;
                        setStatus(state, "Loaded soundfont: %s", fpath.c_str());
                    }
                } else if (ext == ".wav") {
                    pushUndo(state);
                    int chIdx = (int)state.project.channels.size();
                    addChannel(state, true);
                    if (chIdx < (int)state.project.channels.size()) {
                        auto& ch = state.project.channels[chIdx];
                        ch.samplePath = fpath;
                        int patTicks = state.project.ppq * 4;
                        if (state.engine) {
                            int sampleIdx = state.engine->loadSample(fpath);
                            if (sampleIdx >= 0) {
                                ch.sampleIndex = sampleIdx;
                                auto* sd = state.engine->getSample(sampleIdx);
                                if (sd && sd->loaded && !sd->samples.empty()) {
                                    float durSec = (sd->samples.size() / sd->numChannels) / (float)sd->sampleRate;
                                    float secPerTick = 60.0f / (state.project.bpm * state.project.ppq);
                                    int durTicks = (int)(durSec / secPerTick);
                                    patTicks = std::max(durTicks, state.project.ppq);
                                }
                            }
                        }
                        Pattern pat;
                        pat.name = ch.name;
                        pat.lengthTicks = patTicks;
                        pat.channelIndex = chIdx;
                        int patIdx = (int)state.project.patterns.size();
                        state.project.patterns.push_back(pat);
                        PlaylistClip clip;
                        clip.track = chIdx;
                        clip.patternIndex = patIdx;
                        clip.startTick = 0;
                        state.project.playlist.push_back(clip);
                        state.project.modified = true;
                        setStatus(state, "Loaded sample: %s", fpath.c_str());
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
    ImGui::End();
}

void renderMixer(AppState& state) {
    ImGui::Begin("Mixer", &state.showMixer, ImGuiWindowFlags_NoFocusOnAppearing);
    state.project.ensureMixerSlots();
    float stripW = 80.0f;
    float stripH = ImGui::GetContentRegionAvail().y > 300 ? 300 : ImGui::GetContentRegionAvail().y;
    for (int m = 0; m < (int)state.project.mixer.size(); m++) {
        auto& slot = state.project.mixer[m];
        ImGui::BeginGroup();
        ImGui::PushID(m);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f,0.8f,0.8f,1));
        ImGui::Text("%s", slot.name.c_str());
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f,0.15f,0.15f,1));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1,0.67f,0,1));
        float vol = slot.volume;
        ImGui::PushItemWidth(30);
        ImGui::VSliderFloat("##vol", ImVec2(30, stripH - 120), &vol, 0.0f, 1.5f, "");
        ImGui::PopItemWidth();
        ImGui::SetItemTooltip("Mixer volume");
        if (vol != slot.volume) {
            slot.volume = std::clamp(vol, 0.0f, 1.5f);
            state.project.modified = true;
        }
        ImGui::PopStyleColor(2);
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "%.2f", slot.volume);
        ImGui::Text("%s", vbuf);
        float pn = slot.pan;
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##pan", &pn, 0.01f, -1.0f, 1.0f, "Pan");
        ImGui::PopItemWidth();
        ImGui::SetItemTooltip("Pan (L/R balance)");
        if (pn != slot.pan) {
            slot.pan = std::clamp(pn, -1.0f, 1.0f);
            state.project.modified = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, slot.muted ? COL_MUTED : IM_COL32(180,180,180,255));
        if (ImGui::SmallButton("M")) {
            pushUndo(state);
            slot.muted = !slot.muted;
            state.project.modified = true;
        }
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Mute / Unmute");

        ImGui::PopID();
        ImGui::EndGroup();
        ImGui::SameLine();
    }

    ImGui::Separator();
    ImGui::Text("Master");
    float mv = state.masterVolume;
    ImGui::PushItemWidth(60);
    ImGui::SliderFloat("##masterVol", &mv, 0.0f, 1.5f, "Vol: %.2f");
    ImGui::PopItemWidth();
    ImGui::SetItemTooltip("Master volume");
    if (mv != state.masterVolume) {
        state.masterVolume = std::clamp(mv, 0.0f, 1.5f);
        if (state.engine) state.engine->setMasterVolume(state.masterVolume);
    }

    ImGui::End();
}

static std::string normalizePath(const std::string& p) {
    if (p == ".") {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        return cwd;
    }
    return p;
}

void renderBrowser(AppState& state) {
    ImGui::Begin("Browser", &state.showBrowser, ImGuiWindowFlags_NoFocusOnAppearing);
    std::string displayPath = normalizePath(state.browserPath);
    if (displayPath.size() > 60) {
        std::string s = "..." + displayPath.substr(displayPath.size() - 57);
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "%s", s.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "%s", displayPath.c_str());
    }
    ImGui::SetItemTooltip("%s", displayPath.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Home")) {
        refreshBrowser(state, ".");
    }
    ImGui::Separator();
    ImGui::BeginChild("##browserFiles");
    if (state.browserPath != "." && state.browserPath != "/") {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f,0.6f,0.2f,1));
        if (ImGui::Selectable("..  (parent)", false)) {
            std::string parent = normalizePath(state.browserPath);
            size_t slash = parent.rfind('/');
            if (slash != std::string::npos) {
                std::string up = parent.substr(0, slash);
                if (up.empty()) up = "/";
                refreshBrowser(state, up.c_str());
                state.browserSelected = -1;
            }
        }
        ImGui::PopStyleColor();
    }

    for (int i = 0; i < (int)state.browserFiles.size(); i++) {
        bool selected = (i == state.browserSelected);
        std::string label = state.browserFiles[i];
        ImGui::PushID(i);
        if (state.browserIsDir[i]) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.7f,1.0f,1));
            if (ImGui::Selectable(label.c_str(), selected)) {
                state.browserSelected = i;
                std::string base = normalizePath(state.browserPath);
                std::string newPath = base + "/" + state.browserFiles[i];
                refreshBrowser(state, newPath.c_str());
                state.browserSelected = -1;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else {
            if (ImGui::Selectable(label.c_str(), selected)) {
                state.browserSelected = i;
            }
            if (ImGui::BeginDragDropSource()) {
                std::string base = normalizePath(state.browserPath);
                std::string fullPath = base + "/" + state.browserFiles[i];
                ImGui::SetDragDropPayload("SAMPLE_PATH", fullPath.c_str(), fullPath.size() + 1);
                ImGui::Text("%s", state.browserFiles[i].c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();
}

void renderExportDialog(AppState& state) {
    if (!state.showExportDialog) return;
    ImGui::OpenPopup("Export to WAV");
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Export to WAV", &state.showExportDialog)) {
        ImGui::Text("Export your project as a WAV file.");
        ImGui::Separator();
        ImGui::PushItemWidth(300);
        ImGui::InputText("Path", state.exportPath, sizeof(state.exportPath));
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Sample Rate", &state.exportSampleRate, 22050, 192000);
        ImGui::PopItemWidth();
        if (strlen(state.exportPath) == 0) {
            const char* home = getenv("HOME");
            if (home) snprintf(state.exportPath, sizeof(state.exportPath), "%s/export.wav", home);
        }

        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(100, 30))) {
            if (appExportWAV(state, state.exportPath)) {
                state.showExportDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 30))) {
            state.showExportDialog = false;
        }

        ImGui::EndPopup();
    }
}

void renderAboutDialog(AppState& state) {
    if (!state.showAboutDialog) return;
    ImGui::OpenPopup("About neoDAW");
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("About neoDAW", &state.showAboutDialog)) {
        ImGui::TextColored(ImVec4(1,0.67f,0,1), "neoDAW v0.0.0");
        ImGui::Text("A weird digital audio workstation made by");
        ImGui::Text("a weird guy.");
        ImGui::Text("Built with C++, DearImGui and SDL.");
        ImGui::Separator();
        ImGui::Text("Licensed under the MPL-2.0 license.");
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(80, 24))) {
            state.showAboutDialog = false;
        }
        ImGui::EndPopup();
    }
}

void renderConfirmationDialog(AppState& state) {
    if (state.pendingAction == 0) return;
    const char* title = "";
    const char* msg = "";
    switch (state.pendingAction) {
        case 1: title = "Unsaved Changes##new"; msg = "Create new project? Unsaved changes will be lost."; break;
        case 2: title = "Unsaved Changes##quit"; msg = "Quit? Unsaved changes will be lost."; break;
        case 3: title = "Unsaved Changes##open"; msg = "Open project? Unsaved changes will be lost."; break;
    }
    ImGui::OpenPopup(title);
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", msg);
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(80, 0))) {
            if (!state.project.filePath.empty()) {
                appSaveProject(state, state.project.filePath.c_str());
            } else {
                state.actionSaveAs = true;
            }
            int action = state.pendingAction;
            state.pendingAction = 0;
            ImGui::CloseCurrentPopup();
            if (action == 1) {
                state.project = Project();
                state.project.ensureMixerSlots();
                addChannel(state, false);
                addPattern(state);
                state.engine->stopPlayback();
            } else             if (action == 2) {
                state.requestQuit = true;
            } else if (action == 3) {
                state.actionOpen = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(90, 0))) {
            int action = state.pendingAction;
            state.pendingAction = 0;
            ImGui::CloseCurrentPopup();
            if (action == 1) {
                state.project = Project();
                state.project.ensureMixerSlots();
                addChannel(state, false);
                addPattern(state);
                state.engine->stopPlayback();
            } else if (action == 2) {
                state.requestQuit = true;
            } else if (action == 3) {
                state.actionOpen = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            state.pendingAction = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void appRender(AppState& state, float deltaTime) {
    if (state.statusTimer > 0) state.statusTimer -= deltaTime;
    if (state.statusTimer < 0) state.statusTimer = 0;
    renderMainMenu(state);
    ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static bool dockInit = false;
    if (!dockInit && ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        dockInit = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);
        ImGuiID dockMain = dockspaceId;
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);
        ImGui::DockBuilderDockWindow("Browser", dockLeft);
        ImGui::DockBuilderDockWindow("Channel Rack", dockRight);
        ImGui::DockBuilderDockWindow("Mixer", dockBottom);
        ImGui::DockBuilderDockWindow("Playlist", dockMain);
        ImGui::DockBuilderDockWindow("Piano Roll", dockMain);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift && !io.KeyAlt) {
        undo(state);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && !io.KeyAlt) {
        redo(state);
    }
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyAlt) {
        redo(state);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !io.KeyShift && !io.KeyAlt) {
        state.showExportDialog = true;
    }

    renderTransportBar(state);
    renderConfirmationDialog(state);
    if (state.showBrowser) renderBrowser(state);
    if (state.showPlaylist) renderPlaylist(state);
    if (state.showPianoRoll) renderPianoRoll(state);
    if (state.showChannelRack) renderChannelRack(state);
    if (state.showMixer) renderMixer(state);
    renderExportDialog(state);
    renderAboutDialog(state);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SAMPLE_PATH")) {
            std::string path((const char*)payload->Data, payload->DataSize - 1);
            if (!path.empty() && state.project.selectedChannel >= 0 &&
                state.project.selectedChannel < (int)state.project.channels.size()) {
                auto& ch = state.project.channels[state.project.selectedChannel];
                size_t dot = path.rfind('.');
                std::string ext;
                if (dot != std::string::npos) {
                    for (char c : path.substr(dot)) ext += (char)std::tolower((unsigned char)c);
                }
                if (ext == ".sf2") {
                    ch.useSF2 = true;
                    ch.isSampler = false;
                    ch.sf2Path = path;
                    state.project.modified = true;
                    if (state.engine) {
                        state.engine->lockAudio();
                        int sfontId = state.engine->loadSFont(path);
                        if (sfontId >= 0) {
                            int prog = 0;
                            for (int p = 0; p < 128; p++) {
                                if (state.engine->selectSFontPreset(sfontId, 0, p, state.project.selectedChannel)) {
                                    prog = p; break;
                                }
                            }
                            ch.sf2Preset = prog;
                        }
                        state.engine->unlockAudio();
                    }
                    setStatus(state, "Loaded soundfont: %s", path.c_str());
                } else {
                    ch.samplePath = path;
                    ch.isSampler = true;
                    state.project.modified = true;
                    if (state.engine) {
                        int sampleIdx = state.engine->loadSample(path);
                        if (sampleIdx >= 0) ch.sampleIndex = sampleIdx;
                    }
                    setStatus(state, "Loaded sample: %s", path.c_str());
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}
