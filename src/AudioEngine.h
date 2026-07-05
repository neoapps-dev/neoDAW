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
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
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

class AudioEngine : public juce::AudioIODeviceCallback {
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
    void clearSamples();
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
    struct SoundFontPreset {
        int bank = 0;
        int preset = 0;
        std::string name;
    };
    int fluidSfontId = -1;
    int loadSFont(const std::string& path);
    void unloadSFont();
    bool selectSFontPreset(int sfontId, int bank, int preset, int chan);
    std::vector<SoundFontPreset> getSoundFontPresets(int sfontId);
    void playSamplePreview(const std::string& path);

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                          float* const* outputChannelData, int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void render(float* output, unsigned int frameCount);
    static bool loadWavFile(const std::string& path, SampleData& sd);
    static bool writeWavToFile(const std::string& path, const float* interleavedData, int numFrames, int numChannels, int sampleRate);
    Project* project = nullptr;
    Transport* transport = nullptr;
    bool initialized = false;
    std::unique_ptr<juce::AudioDeviceManager> audioDeviceManager;
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
    double tickRemainder = 0.0;
    std::atomic<float> masterVolume{1.0f};
    Synthesizer previewSynth;
    std::mutex previewMutex;
    std::atomic<int> previewKey{-1};
    std::atomic<int> previewVelocity{0};
    std::atomic<int> previewOn{0};
    fluid_settings_t* fluidSettings = nullptr;
    fluid_synth_t* fluidSynth = nullptr;
    int lastMetronomeBeat = -1;
    int metronomeClickSampleRemaining = 0;
    float metronomeClickPhase = 0.0f;
    float metronomeClickFreq = 800.0f;
    float metronomeClickPhaseStep = 0.0f;
    float metronomeClickDecayStep = 0.0f;
    static constexpr int MAX_DELAY_SAMPLES = 88200;
    struct DelayLine {
        std::vector<float> bufferL;
        std::vector<float> bufferR;
        int writePos = 0;
        void init(int maxSamples) {
            bufferL.assign(maxSamples, 0.0f);
            bufferR.assign(maxSamples, 0.0f);
            writePos = 0;
        }
        void write(float l, float r) {
            bufferL[writePos] = l;
            bufferR[writePos] = r;
            writePos = (writePos + 1) % (int)bufferL.size();
        }
        void read(int delaySamples, float& outL, float& outR) const {
            int sz = (int)bufferL.size();
            int pos = (writePos - delaySamples + sz * 2) % sz;
            outL = bufferL[pos];
            outR = bufferR[pos];
        }
        void clear() {
            std::fill(bufferL.begin(), bufferL.end(), 0.0f);
            std::fill(bufferR.begin(), bufferR.end(), 0.0f);
            writePos = 0;
        }
    };
    DelayLine mixerDelays[NUM_MIXER_SLOTS];
    bool mixerDelaysInit = false;
    struct FilterState {
        float l1 = 0.0f, l2 = 0.0f;
        float r1 = 0.0f, r2 = 0.0f;
        void clear() {
            l1 = l2 = r1 = r2 = 0.0f;
        }
    };
    FilterState mixerFilters[NUM_MIXER_SLOTS];
    bool mixerFiltersInit = false;
    struct MasterLimiter {
        float gain = 1.0f;
        float threshold = 0.95f;
        float releaseCoeff = 0.998f;
        void process(float& l, float& r) {
            float peak = std::max(std::abs(l), std::abs(r));
            float targetGain = 1.0f;
            if (peak > threshold) {
                targetGain = threshold / peak;
            }
            if (targetGain < gain) {
                gain = targetGain;
            } else {
                gain = gain * releaseCoeff + targetGain * (1.0f - releaseCoeff);
            }
            l *= gain;
            r *= gain;
        }
        void reset() {
            gain = 1.0f;
        }
    };
    MasterLimiter slotLimiters[NUM_MIXER_SLOTS];
    bool slotLimitersInit = false;
    juce::dsp::Reverb slotReverbs[NUM_MIXER_SLOTS];
    bool slotReverbsInit = false;
    juce::dsp::Chorus<float> slotChoruses[NUM_MIXER_SLOTS];
    bool slotChorusesInit = false;
    struct PreviewSampleState {
        std::vector<float> samples;
        int numChannels = 1;
        double sampleRate = 44100.0;
        std::atomic<double> playPos{-1.0};
    };
    PreviewSampleState previewSample;
    std::mutex previewSampleMutex;
};
