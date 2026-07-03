#include "App.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif
static SDL_Window* gWindow = nullptr;
static SDL_GLContext gGLContext = nullptr;
static AppState gAppState;
static AudioEngine gAudioEngine;
static bool gRunning = true;
static bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    gWindow = SDL_CreateWindow("neoDAW", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!gWindow) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    gGLContext = SDL_GL_CreateContext(gWindow);
    if (!gGLContext) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(gWindow, gGLContext);
    SDL_GL_SetSwapInterval(1);
    return true;
}

static void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "neodaw.ini";
    renderTheme();
    ImGui_ImplSDL2_InitForOpenGL(gWindow, gGLContext);
    ImGui_ImplOpenGL3_Init("#version 330");
}

static void shutdown() {
    gAudioEngine.stopPlayback();
    gAudioEngine.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (gGLContext) {
        SDL_GL_DeleteContext(gGLContext);
        gGLContext = nullptr;
    }
    if (gWindow) {
        SDL_DestroyWindow(gWindow);
        gWindow = nullptr;
    }
    SDL_Quit();
}

static void handleFileOpen() {
#ifdef _WIN32
    char path[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    char filter[] = "neoDAW Project\0*.neodaw\0All Files\0*.*\0";
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Open Project";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameA(&ofn) && strlen(path) > 0) {
        appLoadProject(gAppState, path);
    }
#else
    FILE* fp = popen("zenity --file-selection --title='Open Project' --file-filter='*.neodaw' 2>/dev/null", "r");
    if (!fp) return;
    char path[4096];
    if (fgets(path, sizeof(path), fp)) {
        size_t len = strlen(path);
        if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
        if (strlen(path) > 0) {
            appLoadProject(gAppState, path);
        }
    }
    pclose(fp);
#endif
}

static void handleFileSaveAs() {
#ifdef _WIN32
    char path[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    char filter[] = "neoDAW Project\0*.neodaw\0All Files\0*.*\0";
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Save Project As";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetSaveFileNameA(&ofn) && strlen(path) > 0) {
        if (strcmp(path + strlen(path) - 7, ".neodaw") != 0) strncat(path, ".neodaw", sizeof(path) - strlen(path) - 1);
        appSaveProject(gAppState, path);
    }
#else
    FILE* fp = popen("zenity --file-selection --save --title='Save Project As' --file-filter='*.neodaw' 2>/dev/null", "r");
    if (!fp) return;
    char path[4096];
    if (fgets(path, sizeof(path), fp)) {
        size_t len = strlen(path);
        if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
        if (strlen(path) > 0) {
            if (strcmp(path + strlen(path) - 7, ".neodaw") != 0) strncat(path, ".neodaw", sizeof(path) - strlen(path) - 1);
            appSaveProject(gAppState, path);
        }
    }
    pclose(fp);
#endif
}

static void handleImportMIDI() {
#ifdef _WIN32
    char path[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    char filter[] = "MIDI Files\0*.mid;*.midi\0All Files\0*.*\0";
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Import MIDI";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameA(&ofn) && strlen(path) > 0) {
        appImportMIDI(gAppState, path);
    }
#else
    FILE* fp = popen("zenity --file-selection --title='Import MIDI' --file-filter='*.mid *.midi' 2>/dev/null", "r");
    if (!fp) return;
    char path[4096];
    if (fgets(path, sizeof(path), fp)) {
        size_t len = strlen(path);
        if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
        if (strlen(path) > 0) {
            appImportMIDI(gAppState, path);
        }
    }
    pclose(fp);
#endif
}

int main(int argc, char** argv) {
    if (!initSDL()) return 1;
    AudioSettings settings;
    settings.sampleRate = 44100;
    settings.bufferSize = 512;
    settings.numChannels = 2;
    if (!gAudioEngine.init(settings)) {
        fprintf(stderr, "Warning: Failed to init audio engine (will run without audio)\n");
    }

    gAudioEngine.setProject(&gAppState.project);
    gAudioEngine.setTransport(&gAppState.transport);
    gAudioEngine.setBPM(gAppState.project.bpm);
    gAppState.engine = &gAudioEngine;
    if (argc > 1 && strcmp(argv[1], "--render-debug") == 0) {
        if (argc > 2) appLoadProject(gAppState, argv[2]);
        gAudioEngine.debugRenderToWav(gAppState.project, gAppState.transport, "debug_engine.wav");
        gAudioEngine.debugRenderFreshToWav(gAppState.project, gAppState.transport, "debug_fresh.wav");
        gAudioEngine.debugRenderEngineSingleToWav(gAppState.project, gAppState.transport, "debug_single.wav");
        gAudioEngine.debugRenderPoolFreshLoop(gAppState.project, gAppState.transport, "debug_pool_fresh.wav");
        gAudioEngine.shutdown();
        SDL_Quit();
        return 0;
    }

    if (argc > 1) {
        appLoadProject(gAppState, argv[1]);
    }

    if (!appInit(gAppState, &gAudioEngine)) {
        fprintf(stderr, "Failed to init app state\n");
        shutdown();
        return 1;
    }

    initImGui();
    Uint64 lastTime = SDL_GetPerformanceCounter();
    while (gRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) gRunning = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) gRunning = false;
            if (event.type == SDL_KEYDOWN) {
                bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                if (ctrl && event.key.keysym.sym == SDLK_n) {
                    gAppState.project = Project();
                    gAppState.project.ensureMixerSlots();
                    addChannel(gAppState, false);
                    addPattern(gAppState);
                    gAudioEngine.stopPlayback();
                }
                if (ctrl && event.key.keysym.sym == SDLK_o) handleFileOpen();
                if (ctrl && event.key.keysym.sym == SDLK_s && shift) handleFileSaveAs();
                if (ctrl && event.key.keysym.sym == SDLK_s && !shift) {
                    if (!gAppState.project.filePath.empty())
                        appSaveProject(gAppState, gAppState.project.filePath.c_str());
                    else
                        handleFileSaveAs();
                }
                if (ctrl && event.key.keysym.sym == SDLK_q) {
                    if (gAppState.project.modified)
                        gAppState.pendingAction = 2;
                    else
                        gRunning = false;
                }
                if (event.key.keysym.sym == SDLK_SPACE && !(ctrl)) {
                    if (gAudioEngine.isInitialized())
                        gAudioEngine.pausePlayback();
                }
                if (event.key.keysym.sym == SDLK_DELETE &&
                    gAppState.pianoRollEditingNote >= 0) {
                    int patIdx = gAppState.project.selectedPattern;
                    if (patIdx >= 0 && patIdx < (int)gAppState.project.patterns.size()) {
                        auto& pat = gAppState.project.patterns[patIdx];
                        int noteIdx = gAppState.pianoRollEditingNote;
                        if (noteIdx >= 0 && noteIdx < (int)pat.notes.size()) {
                            pat.notes.erase(pat.notes.begin() + noteIdx);
                            gAppState.pianoRollEditingNote = -1;
                            gAppState.project.modified = true;
                        }
                    }
                }
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float deltaTime = (float)((now - lastTime) / (double)SDL_GetPerformanceFrequency());
        lastTime = now;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        gAudioEngine.setProject(&gAppState.project);
        appRender(gAppState, deltaTime);
        if (gAppState.actionOpen) {
            gAppState.actionOpen = false;
            handleFileOpen();
        }
        if (gAppState.actionSaveAs) {
            gAppState.actionSaveAs = false;
            handleFileSaveAs();
        }
        if (gAppState.actionImportMIDI) {
            gAppState.actionImportMIDI = false;
            handleImportMIDI();
        }

        if (gAppState.requestQuit) {
            gAppState.requestQuit = false;
            gRunning = false;
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(gWindow);
    }

    shutdown();
    return 0;
}
