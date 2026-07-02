#pragma once
#include "Types.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <atomic>
static constexpr int MAX_VOICES = 32;
static constexpr float PI = 3.14159265358979323846f;
static constexpr float TWO_PI = 6.28318530717958647692f;
inline float noteToFreq(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

struct Voice {
    bool active = false;
    int note = 60;
    float velocity = 1.0f;
    float phase = 0.0f;
    float envPhase = 0.0f;
    float envValue = 0.0f;
    int envStage = 0;
    int startTick = 0;
    int releaseTick = 0;
    bool released = false;
    float filterState1 = 0.0f, filterState2 = 0.0f;
    void reset() {
        active = false;
        released = false;
        phase = 0.0f;
        envPhase = 0.0f;
        envValue = 0.0f;
        envStage = 0;
        filterState1 = 0.0f;
        filterState2 = 0.0f;
    }
};

struct EnvelopeParams {
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.7f;
    float release = 0.3f;
};

class Synthesizer {
public:
    void noteOn(int note, float velocity, int startTick);
    void noteOff(int note, int currentTick);
    void allNotesOff();
    void cutAllVoices();
    void forceReset();
    void setEnvelope(const EnvelopeParams& env) { envelope = env; }
    void render(float* buffer, int numFrames, int numChannels, float sampleRate, int playHead, int patternLength, float bpm, int ppq);
    Waveform waveform = Waveform::Sine;
    float filterCutoff = 1.0f;
    float filterResonance = 0.0f;
    FilterType filterType = FilterType::LowPass;
    float volume = 0.8f;
    float pan = 0.0f;

private:
    std::array<Voice, MAX_VOICES> voices;
    int nextVoice = 0;
    EnvelopeParams envelope{};
    int voiceCount = 0;

public:
    const std::array<Voice, MAX_VOICES>& getVoices() const { return voices; }

private:
    int findVoice(int note);
    int allocVoice();
    float renderOscillator(float phase, Waveform wf);
    void processVoice(Voice& v, float sampleRate, float& leftOut, float& rightOut);
};

inline float Synthesizer::renderOscillator(float phase, Waveform wf) {
    phase = std::fmod(phase, 1.0f);
    if (phase < 0.0f) phase += 1.0f;
    switch (wf) {
        case Waveform::Sine:    return std::sin(phase * TWO_PI);
        case Waveform::Saw:     return 2.0f * phase - 1.0f;
        case Waveform::Square:  return phase < 0.5f ? 1.0f : -1.0f;
        case Waveform::Triangle: return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
        case Waveform::Noise:   return 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f;
    }
    return 0.0f;
}

inline void Synthesizer::processVoice(Voice& v, float sampleRate, float& leftOut, float& rightOut) {
    float freq = noteToFreq(v.note);
    float phaseInc = freq / sampleRate;
    v.phase += phaseInc;
    if (v.phase >= 1.0f) v.phase -= 1.0f;
    float osc = renderOscillator(v.phase, waveform);
    float dt = 1.0f / sampleRate;
    float env = 0.0f;
    switch (v.envStage) {
        case 0: {
            v.envPhase += dt;
            float t = v.envPhase / envelope.attack;
            if (t >= 1.0f) { t = 1.0f; v.envStage = 1; v.envPhase = 0.0f; }
            env = t;
            break;
        }
        case 1: {
            v.envPhase += dt;
            float t = v.envPhase / envelope.decay;
            if (t >= 1.0f) { t = 1.0f; v.envStage = 2; }
            env = std::lerp(1.0f, envelope.sustain, t);
            break;
        }
        case 2: { env = envelope.sustain; break; }
        case 3: {
            v.envPhase += dt;
            float t = v.envPhase / envelope.release;
            if (t >= 1.0f) { t = 1.0f; v.envStage = 4; }
            env = std::lerp(envelope.sustain, 0.0f, t);
            break;
        }
        case 4: env = 0.0f; break;
    }
    v.envValue = env;
    float sig = osc * env * v.velocity;
    if (filterType != FilterType::None && filterCutoff < 0.99f) {
        float fc = std::min(filterCutoff * 0.99f, 0.99f);
        float q = 1.0f - std::min(filterResonance, 0.95f);
        float f = 2.0f * std::sin(PI * fc * 0.5f);
        f = std::min(f, 0.99f);
        float hp = sig - v.filterState1 - q * v.filterState2;
        float bp = f * hp + v.filterState1;
        float lp = f * bp + v.filterState2;
        v.filterState1 = bp;
        v.filterState2 = lp;
        switch (filterType) {
            case FilterType::LowPass:  sig = lp; break;
            case FilterType::HighPass: sig = hp; break;
            case FilterType::BandPass: sig = bp; break;
            default: break;
        }
    }

    if (v.envStage == 4) {
        v.active = false;
        voiceCount--;
        return;
    }

    float p = pan * 0.5f + 0.5f;
    leftOut += sig * (1.0f - p) * volume;
    rightOut += sig * p * volume;
}

inline int Synthesizer::findVoice(int note) {
    for (auto& v : voices) if (v.active && v.note == note) return (int)(&v - voices.data());
    return -1;
}

inline int Synthesizer::allocVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
        int idx = (nextVoice + i) % MAX_VOICES;
        if (!voices[idx].active) {
            nextVoice = (idx + 1) % MAX_VOICES;
            return idx;
        }
    }
    int idx = nextVoice;
    nextVoice = (nextVoice + 1) % MAX_VOICES;
    return idx;
}

inline void Synthesizer::noteOn(int note, float velocity, int startTick) {
    int idx = allocVoice();
    voices[idx].reset();
    voices[idx].active = true;
    voices[idx].note = note;
    voices[idx].velocity = velocity;
    voices[idx].phase = 0.0f;
    voices[idx].envPhase = 0.0f;
    voices[idx].envValue = 0.0f;
    voices[idx].envStage = 0;
    voices[idx].startTick = startTick;
    voices[idx].released = false;
    voiceCount++;
}

inline void Synthesizer::noteOff(int note, int currentTick) {
    for (auto& v : voices) {
        if (v.active && v.note == note && !v.released) {
            v.released = true;
            v.releaseTick = currentTick;
            if (v.envStage < 3) {
                v.envStage = 3;
                v.envPhase = 0.0f;
            }
        }
    }
}

inline void Synthesizer::allNotesOff() {
    for (auto& v : voices) {
        if (v.active && !v.released) {
            v.released = true;
            if (v.envStage < 3) {
                v.envStage = 3;
                v.envPhase = 0.0f;
            }
        }
    }
}

inline void Synthesizer::cutAllVoices() {
    for (auto& v : voices) {
        if (v.active) {
            v.active = false;
            voiceCount = std::max(0, voiceCount - 1);
        }
    }
}

inline void Synthesizer::forceReset() {
    for (auto& v : voices) v.reset();
    nextVoice = 0;
    voiceCount = 0;
}

inline void Synthesizer::render(float* buffer, int numFrames, int numChannels, float sampleRate, int playHead, int patternLength, float bpm, int ppq) {
    if (numFrames <= 0) return;
    float secPerTick = 60.0f / (bpm * ppq);
    for (int i = 0; i < numFrames; i++) {
        float leftOut = 0.0f, rightOut = 0.0f;
        for (auto& v : voices) {
            if (!v.active) continue;
            processVoice(v, sampleRate, leftOut, rightOut);
        }

        int idx = i * numChannels;
        buffer[idx] += leftOut;
        if (numChannels > 1) buffer[idx + 1] += rightOut;
        else buffer[idx] += (leftOut + rightOut) * 0.5f;
    }
}
