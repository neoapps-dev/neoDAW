#pragma once
#include "Types.h"
#include "Synth.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <functional>
#include <cstdint>
#include <map>
#include <SDL.h>
#include <fluidlite.h>
struct ClipRenderState {
    std::vector<int> activeNotes;
    void reset() { activeNotes.clear(); }
};
struct SampleData {
    std::vector<float> samples;
    int sampleRate = 44100;
    int numChannels = 1;
    bool loaded = false;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    bool init(const AudioSettings& settings);
    void shutdown();
    bool isInitialized() const { return initialized; }
    void setProject(Project* project) { this->project = project; }
    void setTransport(Transport* transport) { this->transport = transport; }
    void startPlayback();
    void stopPlayback();
    void pausePlayback();
    int getPlayHead() const { return playHead.load(); }
    float getBPM() const { return bpm.load(); }
    void setBPM(float b) { bpm.store(b); }
    float getMasterVolume() const { return masterVolume.load(); }
    void setMasterVolume(float v) { masterVolume.store(v); }
    int loadSample(const std::string& path);
    const SampleData* getSample(int index) const;
    void previewNoteOn(int key, float velocity = 100.0f);
    void previewNoteOff(int key);
    using RenderCallback = std::function<void(float*, int, int)>;
    void setExternalRenderCallback(RenderCallback cb) { renderCallback = cb; }
    void lockAudio() { audioMutex.lock(); }
    void unlockAudio() { audioMutex.unlock(); }
    std::vector<int>& getActiveNotes() { return activeNotes; }
    bool debugRenderToWav(const Project& proj, const Transport& trans, const char* path);
    bool renderToWav(const Project& proj, const Transport& trans, const char* path, int outSampleRate = 44100);
    bool debugRenderFreshToWav(const Project& proj, const Transport& trans, const char* path);
    int previewChannel = -1;
    bool debugRenderEngineSingleToWav(const Project& proj, const Transport& trans, const char* path);
    bool debugRenderPoolFreshLoop(const Project& proj, const Transport& trans, const char* path);
    int fluidSfontId = -1;
    int loadSFont(const std::string& path);
    void unloadSFont();
    bool selectSFontPreset(int sfontId, int bank, int preset, int chan);

private:
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void render(float* output, unsigned int frameCount);
    Project* project = nullptr;
    Transport* transport = nullptr;
    bool initialized = false;
    SDL_AudioDeviceID audioDevice = 0;
    std::atomic<int> playHead{0};
    std::atomic<float> bpm{130.0f};
    std::mutex audioMutex;
    std::vector<int> activeNotes;
    std::map<int, ClipRenderState> clipRenderStates;
    bool prevPlaylistActive = false;
    std::vector<SampleData> samples;
    Synthesizer synthPool[16];
    RenderCallback renderCallback;
    int sampleRate = 44100;
    int numChannels = 2;
    double playbackTime = 0.0;
    std::atomic<float> masterVolume{1.0f};
    Synthesizer previewSynth;
    std::mutex previewMutex;
    std::atomic<int> previewKey{-1};
    std::atomic<int> previewVelocity{0};
    std::atomic<int> previewOn{0};
    fluid_settings_t* fluidSettings = nullptr;
    fluid_synth_t* fluidSynth = nullptr;
};
