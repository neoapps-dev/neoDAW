#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <atomic>
#include <mutex>
struct Note {
    int start = 0;
    int length = 480;
    int key = 60;
    int velocity = 100;
};

enum class Waveform { Sine, Saw, Square, Triangle, Noise };
enum class FilterType { LowPass, HighPass, BandPass, None };
struct Channel {
    std::string name = "Channel";
    bool isSampler = false;
    Waveform waveform = Waveform::Sine;
    float attack = 0.01f, decay = 0.1f, sustain = 0.7f, release = 0.3f;
    float filterCutoff = 1.0f, filterResonance = 0.0f;
    FilterType filterType = FilterType::LowPass;
    std::string samplePath;
    float sampleStart = 0.0f, sampleEnd = 1.0f;
    int sampleIndex = -1;
    bool useSF2 = false;
    std::string sf2Path;
    int sf2Preset = 0;
    float volume = 0.8f, pan = 0.0f;
    int mixerChannel = 0;
    bool muted = false, soloed = false;
};

struct Pattern {
    std::string name = "Pattern";
    std::vector<Note> notes;
    uint32_t color = 0xFF4488AA;
    int lengthTicks = 1920;
    int channelIndex = 0;
};

struct MixerSlot {
    std::string name = "Mixer";
    float volume = 0.8f, pan = 0.0f;
    bool muted = false;
};

struct PlaylistClip {
    int track = 0;
    int startTick = 0;
    int patternIndex = 0;
    bool muted = false;
};

static constexpr int MAX_PPQ = 480;
static constexpr int NUM_MIXER_SLOTS = 8;
struct Project {
    float bpm = 130.0f;
    int ppq = MAX_PPQ;
    int beatsPerBar = 4;
    std::string name = "Untitled";
    std::string filePath;
    bool modified = false;
    std::vector<Pattern> patterns;
    std::vector<Channel> channels;
    std::vector<MixerSlot> mixer;
    std::vector<PlaylistClip> playlist;
    int selectedPattern = -1;
    int selectedChannel = 0;
    int ticksPerBeat() const { return ppq; }
    int ticksPerBar() const { return ppq * beatsPerBar; }
    float secondsPerTick() const { return 60.0f / (bpm * ppq); }
    Channel& getOrCreateChannel(int idx) {
        while ((int)channels.size() <= idx) channels.push_back({});
        return channels[idx];
    }

    void ensureMixerSlots() {
        while ((int)mixer.size() < NUM_MIXER_SLOTS) {
            MixerSlot ms;
            ms.name = "Mixer " + std::to_string(mixer.size());
            mixer.push_back(ms);
        }
    }
};

struct AudioSettings {
    int sampleRate = 44100;
    int bufferSize = 512;
    int numChannels = 2;
};

enum class TransportState { Stopped, Playing, Paused };
struct Transport {
    std::atomic<TransportState> state{TransportState::Stopped};
    std::atomic<int> playHead{0};
    std::atomic<bool> looping{false};
    std::atomic<int> loopStart{0};
    std::atomic<int> loopEnd{0};
};
