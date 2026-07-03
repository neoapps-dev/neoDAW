#include "AudioEngine.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <SDL.h>
static bool loadWavFile(const std::string& path, SampleData& sd) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    auto cleanup = [&]() { fclose(f); };
    char riff[4];
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { cleanup(); return false; }
    uint32_t fileSize;
    fread(&fileSize, 4, 1, f);
    char wave[4];
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { cleanup(); return false; }
    int audioFormat = 0;
    int numChannels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    uint32_t dataSize = 0;
    uint32_t dataOffset = 0;
    bool foundFmt = false, foundData = false;
    while (true) {
        char chunkId[4];
        uint32_t chunkSize;
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            foundFmt = true;
            uint16_t fmt;
            fread(&fmt, 2, 1, f);
            audioFormat = fmt;
            uint16_t ch;
            fread(&ch, 2, 1, f);
            numChannels = ch;
            uint32_t sr;
            fread(&sr, 4, 1, f);
            sampleRate = sr;
            fseek(f, 6, SEEK_CUR);
            uint16_t bps;
            fread(&bps, 2, 1, f);
            bitsPerSample = bps;
            if (audioFormat == 0xFFFE) audioFormat = 1;
            uint32_t fmtRead = 16;
            if (chunkSize > fmtRead) fseek(f, chunkSize - fmtRead, SEEK_CUR);
        } else if (memcmp(chunkId, "data", 4) == 0) {
            foundData = true;
            dataSize = chunkSize;
            dataOffset = ftell(f);
            fseek(f, chunkSize, SEEK_CUR);
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }

    if (!foundFmt || !foundData) { cleanup(); return false; }
    if (audioFormat != 1 && audioFormat != 3) { cleanup(); return false; }
    int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample < 1) bytesPerSample = 1;
    int totalSamples = dataSize / bytesPerSample;
    std::vector<uint8_t> raw(dataSize);
    fseek(f, dataOffset, SEEK_SET);
    fread(raw.data(), 1, dataSize, f);
    fclose(f);
    sd.samples.resize(totalSamples);
    sd.sampleRate = sampleRate;
    sd.numChannels = numChannels;
    sd.loaded = true;
    if (audioFormat == 3 && bitsPerSample == 32) {
        memcpy(sd.samples.data(), raw.data(), totalSamples * sizeof(float));
    } else if (bitsPerSample == 8) {
        for (int i = 0; i < totalSamples; i++) sd.samples[i] = (raw[i] / 128.0f) - 1.0f;
    } else if (bitsPerSample == 16) {
        auto* src = (int16_t*)raw.data();
        for (int i = 0; i < totalSamples; i++) sd.samples[i] = src[i] / 32768.0f;
    } else if (bitsPerSample == 24) {
        for (int i = 0; i < totalSamples; i++) {
            uint32_t u = raw[i * 3] | (raw[i * 3 + 1] << 8) | (raw[i * 3 + 2] << 16);
            int32_t val = (int32_t)(u ^ 0x800000) - 0x800000;
            sd.samples[i] = val / 8388608.0f;
        }
    } else if (bitsPerSample == 32) {
        auto* src = (int32_t*)raw.data();
        for (int i = 0; i < totalSamples; i++) sd.samples[i] = src[i] / 2147483648.0f;
    }

    return true;
}

AudioEngine::AudioEngine() {}
AudioEngine::~AudioEngine() { shutdown(); }
bool AudioEngine::init(const AudioSettings& settings) {
    if (initialized) return true;
    sampleRate = settings.sampleRate;
    numChannels = settings.numChannels;
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = sampleRate;
    want.format = AUDIO_F32;
    want.channels = numChannels;
    want.samples = settings.bufferSize;
    want.callback = audioCallback;
    want.userdata = this;
    SDL_AudioSpec have;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (audioDevice == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    if (have.format != AUDIO_F32) {
        fprintf(stderr, "SDL audio device returned format 0x%x, AUDIO_F32 required\n", have.format);
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
        return false;
    }

    sampleRate = have.freq;
    numChannels = have.channels;
    fluidSettings = new_fluid_settings();
    if (fluidSettings) {
        fluid_set_log_function(FLUID_WARN, NULL, NULL);
        fluid_set_log_function(FLUID_INFO, NULL, NULL);
        fluidSynth = new_fluid_synth(fluidSettings);
    }

    SDL_PauseAudioDevice(audioDevice, 0);
    initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (fluidSynth) { delete_fluid_synth(fluidSynth); fluidSynth = nullptr; }
    if (fluidSettings) { delete_fluid_settings(fluidSettings); fluidSettings = nullptr; }
    fluidSfontId = -1;
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    initialized = false;
}

void AudioEngine::startPlayback() {
    if (!initialized || !transport) return;
    for (auto& s : synthPool) s.forceReset();
    activeNotes.clear();
    clipRenderStates.clear();
    playHead.store(0);
    lastMetronomeBeat = -1;
    metronomeClickSampleRemaining = 0;
    previewSample.playPos.store(-1.0);
    if (mixerFiltersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerFilters[i].clear();
    }
    if (slotLimitersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotLimiters[i].reset();
    }
    transport->state.store(TransportState::Playing);
}

void AudioEngine::stopPlayback() {
    if (!transport) return;
    transport->state.store(TransportState::Stopped);
    playHead.store(0);
    playbackTime = 0.0;
    lastMetronomeBeat = -1;
    metronomeClickSampleRemaining = 0;
    previewSample.playPos.store(-1.0);
    for (auto& s : synthPool) s.forceReset();
    activeNotes.clear();
    clipRenderStates.clear();
    if (mixerFiltersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerFilters[i].clear();
    }
    if (slotLimitersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotLimiters[i].reset();
    }
}

void AudioEngine::pausePlayback() {
    if (!transport) return;
    TransportState cur = transport->state.load();
    lastMetronomeBeat = -1;
    metronomeClickSampleRemaining = 0;
    previewSample.playPos.store(-1.0);
    if (mixerFiltersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerFilters[i].clear();
    }
    if (slotLimitersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotLimiters[i].reset();
    }
    if (cur == TransportState::Playing) {
        transport->state.store(TransportState::Paused);
    } else {
        if (cur == TransportState::Stopped) {
            for (auto& s : synthPool) s.forceReset();
            activeNotes.clear();
            clipRenderStates.clear();
        }
        transport->state.store(TransportState::Playing);
    }
}

int AudioEngine::loadSample(const std::string& path) {
    SampleData sd;
    if (!loadWavFile(path, sd)) return -1;
    std::lock_guard<std::mutex> lock(audioMutex);
    int idx = (int)samples.size();
    samples.push_back(std::move(sd));
    return idx;
}

void AudioEngine::clearSamples() {
    std::lock_guard<std::mutex> lock(audioMutex);
    samples.clear();
}

void AudioEngine::playSamplePreview(const std::string& path) {
    SampleData sd;
    if (!loadWavFile(path, sd)) return;
    
    std::lock_guard<std::mutex> lock(previewSampleMutex);
    previewSample.samples = std::move(sd.samples);
    previewSample.numChannels = sd.numChannels;
    previewSample.sampleRate = sd.sampleRate;
    previewSample.playPos.store(0.0);
}

const SampleData* AudioEngine::getSample(int index) const {
    if (index < 0 || index >= (int)samples.size()) return nullptr;
    return &samples[index];
}

void AudioEngine::previewNoteOn(int key, float velocity) {
    if (previewChannel >= 0 && fluidSynth && project &&
        previewChannel < (int)project->channels.size() &&
        project->channels[previewChannel].useSF2) {
        previewKey.store(key);
        previewVelocity.store((int)velocity);
        previewOn.store(1);
    } else {
        std::lock_guard<std::mutex> lock(previewMutex);
        previewSynth.noteOn(key, velocity / 127.0f, 0);
    }
}

void AudioEngine::previewNoteOff(int key) {
    if (previewChannel >= 0 && fluidSynth && project &&
        previewChannel < (int)project->channels.size() &&
        project->channels[previewChannel].useSF2) {
        previewKey.store(key);
        previewOn.store(0);
    } else {
        std::lock_guard<std::mutex> lock(previewMutex);
        previewSynth.noteOff(key, 0);
    }
}

int AudioEngine::loadSFont(const std::string& path) {
    if (!fluidSynth) return -1;
    unloadSFont();
    int sfontId = fluid_synth_sfload(fluidSynth, path.c_str(), 1);
    if (sfontId >= 0) fluidSfontId = sfontId;
    return sfontId;
}

void AudioEngine::unloadSFont() {
    if (fluidSynth && fluidSfontId >= 0) {
        fluid_synth_sfunload(fluidSynth, fluidSfontId, 1);
        fluidSfontId = -1;
    }
}

bool AudioEngine::selectSFontPreset(int sfontId, int bank, int preset, int chan) {
    if (!fluidSynth) return false;
    return fluid_synth_program_select(fluidSynth, chan, sfontId, bank, preset) == 0;
}

std::vector<AudioEngine::SoundFontPreset> AudioEngine::getSoundFontPresets(int sfontId) {
    std::vector<SoundFontPreset> list;
    if (!fluidSynth || sfontId < 0) return list;
    
    lockAudio();
    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(fluidSynth, sfontId);
    if (sfont) {
        sfont->iteration_start(sfont);
        fluid_preset_t preset;
        std::memset(&preset, 0, sizeof(preset));
        while (sfont->iteration_next(sfont, &preset)) {
            SoundFontPreset info;
            info.name = preset.get_name(&preset);
            info.bank = preset.get_banknum(&preset);
            info.preset = preset.get_num(&preset);
            list.push_back(info);
        }
    }
    unlockAudio();
    return list;
}

bool AudioEngine::debugRenderToWav(const Project& proj, const Transport& trans, const char* path) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * sampleRate);
    if (numFrames <= 0) return false;
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    int frameIdx = 0;
    Project* saveProject = project;
    Transport* saveTransport = transport;
    project = const_cast<Project*>(&proj);
    transport = const_cast<Transport*>(&trans);
    transport->state.store(TransportState::Playing);
    transport->playHead.store(0);
    for (auto& s : synthPool) s.forceReset();
    playHead.store(0);
    activeNotes.clear();
    bpm.store(proj.bpm);
    bool wasPlaying = audioDevice != 0 && SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING;
    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 1);
    const int chunkSize = 512;
    while (frameIdx < numFrames) {
        int todo = std::min(chunkSize, numFrames - frameIdx);
        render(mix.data() + (size_t)frameIdx * numChannels, (unsigned int)todo);
        frameIdx += todo;
    }

    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0);
    transport->state.store(TransportState::Stopped);
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t dataBytes = (uint32_t)(mix.size() * sizeof(float));
    f.write("RIFF", 4);
    write32(36 + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    write32(16);
    write16(3);
    write16(numChannels);
    write32(sampleRate);
    write32(sampleRate * numChannels * (uint32_t)sizeof(float));
    write16(numChannels * (uint16_t)sizeof(float));
    write16(32);
    f.write("data", 4);
    write32(dataBytes);
    f.write((const char*)mix.data(), dataBytes);
    return true;
}

bool AudioEngine::renderToWav(const Project& proj, const Transport& trans, const char* path, int outSampleRate) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * outSampleRate);
    if (numFrames <= 0) return false;
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    int frameIdx = 0;
    Project* saveProject = project;
    Transport* saveTransport = transport;
    int saveSampleRate = sampleRate;
    project = const_cast<Project*>(&proj);
    transport = const_cast<Transport*>(&trans);
    sampleRate = outSampleRate;
    transport->state.store(TransportState::Playing);
    transport->playHead.store(0);
    for (auto& s : synthPool) s.forceReset();
    if (fluidSynth) {
        for (int c = 0; c < 16; c++) {
            fluid_synth_cc(fluidSynth, c, 123, 0);
        }
    }
    playHead.store(0);
    activeNotes.clear();
    bpm.store(proj.bpm);
    bool wasPlaying = audioDevice != 0 && SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING;
    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 1);
    const int chunkSize = 512;
    while (frameIdx < numFrames) {
        int todo = std::min(chunkSize, numFrames - frameIdx);
        render(mix.data() + (size_t)frameIdx * numChannels, (unsigned int)todo);
        frameIdx += todo;
    }
    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0);
    transport->state.store(TransportState::Stopped);
    sampleRate = saveSampleRate;
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t dataBytes = (uint32_t)(mix.size() * sizeof(float));
    f.write("RIFF", 4);
    write32(36 + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    write32(16);
    write16(3);
    write16(numChannels);
    write32(outSampleRate);
    write32(outSampleRate * numChannels * (uint32_t)sizeof(float));
    write16(numChannels * (uint16_t)sizeof(float));
    write16(32);
    f.write("data", 4);
    write32(dataBytes);
    f.write((const char*)mix.data(), dataBytes);
    return true;
}

bool AudioEngine::debugRenderFreshToWav(const Project& proj, const Transport& trans, const char* path) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * sampleRate);
    if (numFrames <= 0) return false;
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    Synthesizer freshSynth;
    int selPat = proj.selectedPattern;
    if (selPat < 0 || selPat >= (int)proj.patterns.size()) selPat = 0;
    auto& pattern = proj.patterns[selPat];
    int chanIdx = pattern.channelIndex;
    if (chanIdx < 0 || chanIdx >= (int)proj.channels.size()) return false;
    auto& ch = proj.channels[chanIdx];
    if (ch.muted) return false;
    EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
    freshSynth.setEnvelope(env);
    freshSynth.waveform = ch.waveform;
    freshSynth.filterCutoff = ch.filterCutoff;
    freshSynth.filterResonance = ch.filterResonance;
    freshSynth.filterType = ch.filterType;
    freshSynth.volume = ch.volume;
    freshSynth.pan = ch.pan;
    int currentTick = 0;
    int processedFrames = 0;
    float ticksPerFrame = proj.bpm * proj.ppq / (60.0f * sampleRate);
    std::vector<int> activeKeys;
    while (processedFrames < numFrames) {
        int chunkFrames = std::min(numFrames - processedFrames, 512);
        float temp[1024];
        std::fill(temp, temp + chunkFrames * numChannels, 0.0f);
        std::vector<int> toTurnOn;
        std::vector<int> toTurnOff;
        for (auto& note : pattern.notes) {
            int noteEnd = note.start + note.length;
            if (note.start <= currentTick && noteEnd > currentTick) {
                if (std::find(activeKeys.begin(), activeKeys.end(), note.key) == activeKeys.end()) toTurnOn.push_back(note.key);
            } else if (noteEnd <= currentTick) {
                toTurnOff.push_back(note.key);
            }
        }

        for (int k : toTurnOn) {
            activeKeys.push_back(k);
            freshSynth.noteOn(k, 100 / 127.0f, currentTick);
        }
        for (int k : toTurnOff) {
            auto it = std::find(activeKeys.begin(), activeKeys.end(), k);
            if (it != activeKeys.end()) {
                activeKeys.erase(it);
                freshSynth.noteOff(k, currentTick);
            }
        }

        freshSynth.render(temp, chunkFrames, numChannels, (float)sampleRate, currentTick, pattern.lengthTicks, proj.bpm, proj.ppq);
        for (int f = 0; f < chunkFrames * numChannels; f++) mix[(size_t)processedFrames * numChannels + f] += temp[f];
        currentTick += (int)(chunkFrames * ticksPerFrame);
        processedFrames += chunkFrames;
    }

    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t dataBytes = (uint32_t)(mix.size() * sizeof(float));
    f.write("RIFF", 4); write32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(16); write16(3); write16(numChannels);
    write32(sampleRate); write32(sampleRate * numChannels * (uint32_t)sizeof(float));
    write16(numChannels * (uint16_t)sizeof(float)); write16(32);
    f.write("data", 4); write32(dataBytes);
    f.write((const char*)mix.data(), dataBytes);
    return true;
}

bool AudioEngine::debugRenderPoolFreshLoop(const Project& proj, const Transport& trans, const char* path) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * sampleRate);
    if (numFrames <= 0) return false;
    bool wasPlaying = audioDevice != 0 && SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING;
    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 1);
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    for (auto& s : synthPool) s.forceReset();
    int selPat = proj.selectedPattern;
    if (selPat < 0 || selPat >= (int)proj.patterns.size()) selPat = 0;
    auto& pattern = proj.patterns[selPat];
    int chanIdx = pattern.channelIndex;
    if (chanIdx < 0 || chanIdx >= 16) { if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0); return false; }
    auto& ch = proj.channels[chanIdx];
    if (ch.muted) { if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0); return false; }
    EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
    synthPool[chanIdx].setEnvelope(env);
    synthPool[chanIdx].waveform = ch.waveform;
    synthPool[chanIdx].filterCutoff = ch.filterCutoff;
    synthPool[chanIdx].filterResonance = ch.filterResonance;
    synthPool[chanIdx].filterType = ch.filterType;
    synthPool[chanIdx].volume = ch.volume;
    synthPool[chanIdx].pan = ch.pan;
    int currentTick = 0;
    int processedFrames = 0;
    float ticksPerFrame = proj.bpm * proj.ppq / (60.0f * sampleRate);
    std::vector<int> activeKeys;
    while (processedFrames < numFrames) {
        int chunkFrames = std::min(numFrames - processedFrames, 512);
        float temp[1024];
        std::fill(temp, temp + chunkFrames * numChannels, 0.0f);
        std::vector<int> toTurnOn;
        std::vector<int> toTurnOff;
        for (auto& note : pattern.notes) {
            int noteEnd = note.start + note.length;
            if (note.start <= currentTick && noteEnd > currentTick) {
                if (std::find(activeKeys.begin(), activeKeys.end(), note.key) == activeKeys.end()) toTurnOn.push_back(note.key);
            } else if (noteEnd <= currentTick) {
                toTurnOff.push_back(note.key);
            }
        }

        for (int k : toTurnOn) {
            activeKeys.push_back(k);
            synthPool[chanIdx].noteOn(k, 100 / 127.0f, currentTick);
        }
        for (int k : toTurnOff) {
            auto it = std::find(activeKeys.begin(), activeKeys.end(), k);
            if (it != activeKeys.end()) {
                activeKeys.erase(it);
                synthPool[chanIdx].noteOff(k, currentTick);
            }
        }

        synthPool[chanIdx].render(temp, chunkFrames, numChannels, (float)sampleRate, currentTick, pattern.lengthTicks, proj.bpm, proj.ppq);
        for (int f = 0; f < chunkFrames * numChannels; f++) mix[(size_t)processedFrames * numChannels + f] += temp[f];
        currentTick += (int)(chunkFrames * ticksPerFrame);
        processedFrames += chunkFrames;
    }

    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0);
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t dataBytes = (uint32_t)(mix.size() * sizeof(float));
    f.write("RIFF", 4); write32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(16); write16(3); write16(numChannels);
    write32(sampleRate); write32(sampleRate * numChannels * (uint32_t)sizeof(float));
    write16(numChannels * (uint16_t)sizeof(float)); write16(32);
    f.write("data", 4); write32(dataBytes);
    f.write((const char*)mix.data(), dataBytes);
    return true;
}

bool AudioEngine::debugRenderEngineSingleToWav(const Project& proj, const Transport& trans, const char* path) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * sampleRate);
    if (numFrames <= 0) return false;
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    int frameIdx = 0;
    Project* saveProject = project;
    Transport* saveTransport = transport;
    project = const_cast<Project*>(&proj);
    transport = const_cast<Transport*>(&trans);
    transport->state.store(TransportState::Playing);
    transport->playHead.store(0);
    for (auto& s : synthPool) s.forceReset();
    playHead.store(0);
    activeNotes.clear();
    bool wasPlaying = audioDevice != 0 && SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING;
    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 1);
    double currentPlayHead = 0.0;
    float currentBPM = proj.bpm;
    int ppq = proj.ppq;
    double ticksPerFrame = currentBPM * (double)ppq / (60.0 * sampleRate);
    int selectedPatIdx = project->selectedPattern;
    if (selectedPatIdx < 0 || selectedPatIdx >= (int)project->patterns.size()) { project = saveProject; transport = saveTransport; return false; }
    auto& pattern = project->patterns[selectedPatIdx];
    int patLen = std::max(pattern.lengthTicks, 1);
    int totalFrames = numFrames;
    int processedFrames = 0;
    while (processedFrames < totalFrames) {
        int framesToProcess = std::min(totalFrames - processedFrames, 512);
        int intPlayHead = (int)currentPlayHead;
        if (intPlayHead >= patLen) {
            transport->state.store(TransportState::Stopped);
            break;
        }

        struct NoteEvent { int key; int noteIdx; int start; int end; };
        std::vector<NoteEvent> pendingNoteOns;
        std::vector<int> pendingNoteOffs;
        for (int ni = 0; ni < (int)pattern.notes.size(); ni++) {
            auto& note = pattern.notes[ni];
            int noteEnd = note.start + note.length;
            if (note.start <= intPlayHead && noteEnd > intPlayHead)
                pendingNoteOns.push_back({note.key, ni, note.start, noteEnd});
            else if (noteEnd <= intPlayHead)
                pendingNoteOffs.push_back(note.key);
        }

        for (int k : pendingNoteOffs) {
            auto it = std::find(activeNotes.begin(), activeNotes.end(), k);
            if (it != activeNotes.end()) {
                activeNotes.erase(it);
                int chanIdx = pattern.channelIndex;
                if (chanIdx >= 0 && chanIdx < 16)
                    synthPool[chanIdx].noteOff(k, intPlayHead);
            }
        }

        for (auto& ne : pendingNoteOns) {
            bool alreadyActive = false;
            for (int an : activeNotes)
                if (an == ne.key) { alreadyActive = true; break; }
            if (!alreadyActive) {
                activeNotes.push_back(ne.key);
                int chanIdx = pattern.channelIndex;
                if (chanIdx < 16 && chanIdx >= 0) {
                    Channel& ch = project->getOrCreateChannel(chanIdx);
                    EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
                    synthPool[chanIdx].setEnvelope(env);
                    synthPool[chanIdx].waveform = ch.waveform;
                    synthPool[chanIdx].filterCutoff = ch.filterCutoff;
                    synthPool[chanIdx].filterResonance = ch.filterResonance;
                    synthPool[chanIdx].filterType = ch.filterType;
                    synthPool[chanIdx].volume = ch.volume;
                    synthPool[chanIdx].pan = ch.pan;
                    if (!ch.muted)
                        synthPool[chanIdx].noteOn(ne.key, 100 / 127.0f, ne.start);
                }
            }
        }

        int chanIdx = pattern.channelIndex;
        if (chanIdx >= 0 && chanIdx < 16) {
            float chBuf[4096];
            int chSamples = framesToProcess * numChannels;
            if (chSamples > 4096) chSamples = 4096;
            std::memset(chBuf, 0, (size_t)chSamples * sizeof(float));
            synthPool[chanIdx].render(chBuf, framesToProcess, numChannels, (float)sampleRate, intPlayHead, patLen, currentBPM, ppq);
            for (int f = 0; f < framesToProcess * numChannels; f++)
                mix[(size_t)processedFrames * numChannels + f] += chBuf[f];
        }

        currentPlayHead += (double)framesToProcess * ticksPerFrame;
        processedFrames += framesToProcess;
    }

    if (wasPlaying) SDL_PauseAudioDevice(audioDevice, 0);
    transport->state.store(TransportState::Stopped);
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t dataBytes = (uint32_t)(mix.size() * sizeof(float));
    f.write("RIFF", 4); write32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(16); write16(3); write16(numChannels);
    write32(sampleRate); write32(sampleRate * numChannels * (uint32_t)sizeof(float));
    write16(numChannels * (uint16_t)sizeof(float)); write16(32);
    f.write("data", 4); write32(dataBytes);
    f.write((const char*)mix.data(), dataBytes);
    return true;
}

void AudioEngine::audioCallback(void* userdata, Uint8* stream, int len) {
    auto* engine = (AudioEngine*)userdata;
    if (!engine) return;
    int nc = engine->numChannels;
    if (nc < 1 || nc > 8) { std::memset(stream, 0, (size_t)len); return; }
    int frameCount = len / (int)(nc * sizeof(float));
    if (frameCount <= 0 || frameCount > 8192) { std::memset(stream, 0, (size_t)len); return; }
    std::memset(stream, 0, (size_t)len);
    engine->render((float*)stream, (unsigned int)frameCount);
    if (engine->previewMutex.try_lock()) {
        float previewBuf[4096];
        int prevFrames = std::min(frameCount, (int)(sizeof(previewBuf) / sizeof(float) / nc));
        if (prevFrames > 0) {
            std::memset(previewBuf, 0, (size_t)prevFrames * nc * sizeof(float));
            engine->previewSynth.render(previewBuf, prevFrames, nc, (float)engine->sampleRate, 0, 999999, 120, 480);
            float* out = (float*)stream;
            for (int i = 0; i < prevFrames * nc; i++) out[i] += previewBuf[i];
        }
        engine->previewMutex.unlock();
    }
}

void AudioEngine::render(float* output, unsigned int frameCount) {
    if (!project || !transport) return;
    TransportState state = transport->state.load();
    if (state != TransportState::Playing) {
        playHead.store(transport->playHead.load());
        return;
    }

    if (!audioMutex.try_lock()) return;
    if (previewChannel >= 0 && fluidSynth && project &&
        previewChannel < (int)project->channels.size() &&
        project->channels[previewChannel].useSF2) {
        int pk = previewKey.load();
        if (pk >= 0) {
            if (previewOn.load()) fluid_synth_noteon(fluidSynth, previewChannel, pk, previewVelocity.load());
            else fluid_synth_noteoff(fluidSynth, previewChannel, pk);
            previewKey.store(-1);
        }
    }
    int currentTick = playHead.load();
    float currentBPM = project->bpm;
    int ppq = project->ppq;
    float ticksPerFrame = currentBPM * (float)ppq / (60.0f * sampleRate);
    float mv = masterVolume.load();
    bool hasPlaylist = !project->playlist.empty();
    if (hasPlaylist != prevPlaylistActive) {
        for (auto& s : synthPool) s.forceReset();
        activeNotes.clear();
        clipRenderStates.clear();
        prevPlaylistActive = hasPlaylist;
    }

    project->ensureMixerSlots();

    // Lazy-init delay lines
    if (!mixerDelaysInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerDelays[i].init(MAX_DELAY_SAMPLES);
        mixerDelaysInit = true;
    }

    // Lazy-init filters
    if (!mixerFiltersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerFilters[i].clear();
        mixerFiltersInit = true;
    }

    // Lazy-init limiters
    if (!slotLimitersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotLimiters[i].reset();
        slotLimitersInit = true;
    }

    if (!hasPlaylist) {
        audioMutex.unlock();
        return;
    }

    int totalTicks = ppq * 4;
    for (auto& clip : project->playlist) {
        if (clip.patternIndex >= 0 && clip.patternIndex < (int)project->patterns.size()) {
            int end = clip.startTick + project->patterns[clip.patternIndex].lengthTicks;
            if (end > totalTicks) totalTicks = end;
        }
    }

    auto turnOffAllClipNotes = [&](int clipIdx) {
        auto crsIt = clipRenderStates.find(clipIdx);
        if (crsIt == clipRenderStates.end()) return;
        auto& crs = crsIt->second;
        for (int k : crs.activeNotes) {
            auto& clip = project->playlist[clipIdx];
            int patIdx = clip.patternIndex;
            if (patIdx >= 0 && patIdx < (int)project->patterns.size()) {
                int ci = project->patterns[patIdx].channelIndex;
                if (ci >= 0 && ci < 16) {
                    if (ci < (int)project->channels.size() && project->channels[ci].useSF2) {
                        if (fluidSynth) fluid_synth_noteoff(fluidSynth, ci, k);
                    } else {
                        synthPool[ci].noteOff(k, currentTick);
                    }
                }
            }
        }
        crs.activeNotes.clear();
    };

    auto resetAll = [&]() {
        for (auto& s : synthPool) s.cutAllVoices();
        if (fluidSynth) {
            for (int c = 0; c < 16; c++) {
                fluid_synth_cc(fluidSynth, c, 120, 0);
                fluid_synth_cc(fluidSynth, c, 123, 0);
            }
        }
        activeNotes.clear();
        clipRenderStates.clear();
    };

    if (currentTick >= totalTicks) {
        if (transport->looping.load()) {
            currentTick = 0;
            resetAll();
        } else {
            transport->state.store(TransportState::Stopped);
            playHead.store(0);
            transport->playHead.store(0);
            resetAll();
            audioMutex.unlock();
            return;
        }
    }

    int processedFrames = 0;
    while (processedFrames < (int)frameCount) {
        int chunkFrames = std::min((int)frameCount - processedFrames, 512);
        std::fill(output + processedFrames * numChannels, output + (processedFrames + chunkFrames) * numChannels, 0.0f);
        for (int ci = 0; ci < (int)project->playlist.size(); ci++) {
            auto& clip = project->playlist[ci];
            if (clip.muted) continue;
            int patIdx = clip.patternIndex;
            if (patIdx < 0 || patIdx >= (int)project->patterns.size()) continue;
            auto& pat = project->patterns[patIdx];
            int patLen = pat.lengthTicks;
            if (patLen <= 0) continue;
            int clipEnd = clip.startTick + patLen;
            if (currentTick < clip.startTick || currentTick >= clipEnd) {
                turnOffAllClipNotes(ci);
                continue;
            }

            int localTick = currentTick - clip.startTick;
            int chanIdx = pat.channelIndex;
            if (chanIdx < 0 || chanIdx >= 16) continue;
            Channel& ch = project->getOrCreateChannel(chanIdx);
            if (ch.muted) continue;
            float mixVol = 1.0f;
            bool mixMuted = false;
            if (ch.mixerChannel >= 0 && ch.mixerChannel < (int)project->mixer.size()) {
                mixVol = project->mixer[ch.mixerChannel].volume;
                mixMuted = project->mixer[ch.mixerChannel].muted;
            }
            if (mixMuted) continue;
            auto& crs = clipRenderStates[ci];
            std::vector<int> toTurnOn, toTurnOff;
            for (auto& note : pat.notes) {
                int noteEnd = note.start + note.length;
                if (note.start <= localTick && noteEnd > localTick) {
                    if (std::find(crs.activeNotes.begin(), crs.activeNotes.end(), note.key) == crs.activeNotes.end()) toTurnOn.push_back(note.key);
                } else if (noteEnd <= localTick) {
                    toTurnOff.push_back(note.key);
                }
            }

            if (ch.useSF2) {
                for (int k : toTurnOff) {
                    auto it = std::find(crs.activeNotes.begin(), crs.activeNotes.end(), k);
                    if (it != crs.activeNotes.end()) {
                        crs.activeNotes.erase(it);
                        if (fluidSynth) fluid_synth_noteoff(fluidSynth, chanIdx, k);
                    }
                }
                for (int k : toTurnOn) {
                    if (std::find(crs.activeNotes.begin(), crs.activeNotes.end(), k) == crs.activeNotes.end()) {
                        crs.activeNotes.push_back(k);
                        if (fluidSynth) fluid_synth_noteon(fluidSynth, chanIdx, k, 100);
                    }
                }
            } else {
                EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
                synthPool[chanIdx].setEnvelope(env);
                synthPool[chanIdx].waveform = ch.waveform;
                synthPool[chanIdx].filterCutoff = ch.filterCutoff;
                synthPool[chanIdx].filterResonance = ch.filterResonance;
                synthPool[chanIdx].filterType = ch.filterType;
                synthPool[chanIdx].volume = ch.volume * mixVol;
                synthPool[chanIdx].pan = ch.pan;
                for (int k : toTurnOff) {
                    auto it = std::find(crs.activeNotes.begin(), crs.activeNotes.end(), k);
                    if (it != crs.activeNotes.end()) {
                        crs.activeNotes.erase(it);
                        synthPool[chanIdx].noteOff(k, localTick);
                    }
                }
                for (int k : toTurnOn) {
                    if (std::find(crs.activeNotes.begin(), crs.activeNotes.end(), k) == crs.activeNotes.end()) {
                        crs.activeNotes.push_back(k);
                        synthPool[chanIdx].noteOn(k, 100 / 127.0f, localTick);
                    }
                }
                float chBuf[4096];
                int chSamples = chunkFrames * numChannels;
                if (chSamples > 4096) chSamples = 4096;
                std::fill(chBuf, chBuf + chSamples, 0.0f);
                synthPool[chanIdx].render(chBuf, chunkFrames, numChannels, (float)sampleRate, localTick, patLen, currentBPM, ppq);
                for (int f = 0; f < chSamples; f++)
                    output[processedFrames * numChannels + f] += chBuf[f];
            }

            if (ch.isSampler && ch.sampleIndex >= 0 && ch.sampleIndex < (int)samples.size()) {
                auto& sd = samples[ch.sampleIndex];
                if (sd.loaded && !sd.samples.empty()) {
                    float secPerTick = 60.0f / (currentBPM * ppq);
                    double localSec = localTick * secPerTick;
                    int sdChan = sd.numChannels;
                    size_t totalFrames = sd.samples.size() / sdChan;
                    size_t startFrame = (size_t)(ch.sampleStart * totalFrames);
                    size_t endFrame = (size_t)(ch.sampleEnd * totalFrames);
                    size_t playFrames = endFrame - startFrame;
                    if (playFrames > 0) {
                        double sampleRateRatio = sd.sampleRate / (double)sampleRate;
                        double frameOffset = localSec * sd.sampleRate;
                        for (int f = 0; f < chunkFrames; f++) {
                            double pos = std::fmod(frameOffset + f * sampleRateRatio, (double)playFrames);
                            size_t frame = startFrame + (size_t)pos;
                            if (frame >= endFrame) frame = startFrame;
                            size_t sdi = frame * sdChan;
                            float sv = sd.samples[sdi] * ch.volume * mixVol;
                            output[processedFrames * numChannels + f * numChannels] += sv;
                            if (numChannels > 1) {
                                float sr = sdChan > 1 ? sd.samples[sdi + 1] * ch.volume * mixVol : sv;
                                output[processedFrames * numChannels + f * numChannels + 1] += sr;
                            }
                        }
                    }
                }
            }
        }

        if (fluidSynth && numChannels >= 2) {
            for (int ci = 0; ci < (int)project->channels.size() && ci < 16; ci++) {
                auto& ch = project->channels[ci];
                if (!ch.useSF2) continue;
                float mvol = 1.0f;
                bool mmut = false;
                if (ch.mixerChannel >= 0 && ch.mixerChannel < (int)project->mixer.size()) {
                    mvol = project->mixer[ch.mixerChannel].volume;
                    mmut = project->mixer[ch.mixerChannel].muted;
                }
                int volCC = (ch.muted || mmut) ? 0 : (int)(std::clamp(ch.volume * mvol, 0.0f, 1.5f) * 127.0f / 1.5f);
                fluid_synth_cc(fluidSynth, ci, 7, volCC);
                int panCC = (int)((ch.pan * 0.5f + 0.5f) * 127.0f);
                fluid_synth_cc(fluidSynth, ci, 10, std::clamp(panCC, 0, 127));
            }
            float fluidBuf[4096];
            int fluidSamples = chunkFrames * numChannels;
            if (fluidSamples > 4096) fluidSamples = 4096;
            std::fill(fluidBuf, fluidBuf + fluidSamples, 0.0f);
            fluid_synth_write_float(fluidSynth, chunkFrames, fluidBuf, 0, numChannels, fluidBuf, 1, numChannels);
            for (int f = 0; f < fluidSamples; f++)output[processedFrames * numChannels + f] += fluidBuf[f];
        }

        // --- Per-frame stereo feedback delay on each mixer slot ---
        if (numChannels >= 2) {
            for (int f = 0; f < chunkFrames; f++) {
                // Compute current tick for metronome trigger
                int tickOffset = (int)(f * ticksPerFrame);
                int currentFrameTick = currentTick + tickOffset;
                
                int currentBeat = currentFrameTick / ppq;
                if (transport->metronomeEnabled.load() && currentBeat != lastMetronomeBeat) {
                    lastMetronomeBeat = currentBeat;
                    int beatsPerBar = project->beatsPerBar;
                    metronomeClickFreq = ((currentBeat * ppq) % (ppq * beatsPerBar) == 0) ? 1200.0f : 800.0f;
                    metronomeClickSampleRemaining = (int)(0.04f * sampleRate); // 40ms click
                    metronomeClickPhase = 0.0f;
                    metronomeClickPhaseStep = (float)(2.0 * 3.14159265358979323846 * metronomeClickFreq / sampleRate);
                    metronomeClickDecayStep = 1.0f / metronomeClickSampleRemaining;
                }
                
                float metronomeVal = 0.0f;
                if (metronomeClickSampleRemaining > 0) {
                    float amp = metronomeClickSampleRemaining * metronomeClickDecayStep;
                    metronomeVal = sinf(metronomeClickPhase) * amp * 0.15f;
                    metronomeClickPhase += metronomeClickPhaseStep;
                    metronomeClickSampleRemaining--;
                }

                // Render preview sample if active (dry bypass)
                float prevSampleValL = 0.0f;
                float prevSampleValR = 0.0f;
                {
                    std::lock_guard<std::mutex> lock(previewSampleMutex);
                    double playPosVal = previewSample.playPos.load();
                    if (playPosVal >= 0.0) {
                        int previewSize = (int)(previewSample.samples.size() / previewSample.numChannels);
                        double ratio = previewSample.sampleRate / (double)sampleRate;
                        
                        double currentPos = playPosVal;
                        size_t idx1 = (size_t)currentPos;
                        size_t idx2 = idx1 + 1;
                        if (idx2 >= (size_t)previewSize) idx2 = idx1;
                        
                        if (idx1 < (size_t)previewSize) {
                            float t = (float)(currentPos - idx1);
                            int chCount = previewSample.numChannels;
                            
                            float s1_L = previewSample.samples[idx1 * chCount];
                            float s2_L = previewSample.samples[idx2 * chCount];
                            prevSampleValL = (s1_L + t * (s2_L - s1_L)) * 0.6f;
                            
                            if (chCount > 1) {
                                float s1_R = previewSample.samples[idx1 * chCount + 1];
                                float s2_R = previewSample.samples[idx2 * chCount + 1];
                                prevSampleValR = (s1_R + t * (s2_R - s1_R)) * 0.6f;
                            } else {
                                prevSampleValR = prevSampleValL;
                            }
                            
                            playPosVal += ratio;
                            if (playPosVal >= previewSize) {
                                playPosVal = -1.0;
                            }
                            previewSample.playPos.store(playPosVal);
                        } else {
                            previewSample.playPos.store(-1.0);
                        }
                    }
                }

                int idx = (processedFrames + f) * numChannels;
                float dryL = output[idx];
                float dryR = output[idx + 1];
                float wetL = 0.0f, wetR = 0.0f;

                // Process insert slots (1 to 7) first for delay, filtering, and limiting
                for (int mi = 1; mi < (int)project->mixer.size() && mi < NUM_MIXER_SLOTS; mi++) {
                    auto& slot = project->mixer[mi];
                    
                    // Filter slot input if filter is enabled
                    float slotSigL = dryL;
                    float slotSigR = dryR;
                    if (slot.filterEnabled) {
                        float fc = std::clamp(slot.filterCutoff, 0.01f, 0.99f);
                        float q = 1.0f - std::clamp(slot.filterResonance, 0.0f, 0.95f);
                        float fCoeff = 2.0f * sinf(3.14159265f * fc * 0.5f);
                        fCoeff = std::clamp(fCoeff, 0.01f, 0.99f);
                        
                        // Left
                        float hpL = slotSigL - mixerFilters[mi].l1 - q * mixerFilters[mi].l2;
                        float bpL = fCoeff * hpL + mixerFilters[mi].l1;
                        float lpL = fCoeff * bpL + mixerFilters[mi].l2;
                        mixerFilters[mi].l1 = bpL;
                        mixerFilters[mi].l2 = lpL;
                        
                        // Right
                        float hpR = slotSigR - mixerFilters[mi].r1 - q * mixerFilters[mi].r2;
                        float bpR = fCoeff * hpR + mixerFilters[mi].r1;
                        float lpR = fCoeff * bpR + mixerFilters[mi].r2;
                        mixerFilters[mi].r1 = bpR;
                        mixerFilters[mi].r2 = lpR;
                        
                        slotSigL = slot.filterIsHP ? hpL : lpL;
                        slotSigR = slot.filterIsHP ? hpR : lpR;
                    }
                    
                    // Limit insert slot if enabled
                    if (slot.limiterEnabled) {
                        slotLimiters[mi].process(slotSigL, slotSigR);
                    }
                    
                    if (slot.delayEnabled) {
                        int delaySamples = std::clamp((int)(slot.delayTime * sampleRate), 1, MAX_DELAY_SAMPLES - 1);
                        float fb = std::clamp(slot.delayFeedback, 0.0f, 0.95f);
                        float wet = std::clamp(slot.delayWet, 0.0f, 1.0f);
                        float tapL, tapR;
                        mixerDelays[mi].read(delaySamples, tapL, tapR);
                        if (slot.delayPingPong) {
                            mixerDelays[mi].write(slotSigL + tapR * fb, slotSigR + tapL * fb);
                        } else {
                            mixerDelays[mi].write(slotSigL + tapL * fb, slotSigR + tapR * fb);
                        }
                        wetL += tapL * wet;
                        wetR += tapR * wet;
                    }
                }
                
                // Sum dry and delay outputs
                float outL = (dryL + wetL) * mv;
                float outR = (dryR + wetR) * mv;
                
                // Process Master slot (0) filter on the final mix
                auto& masterSlot = project->mixer[0];
                if (masterSlot.filterEnabled) {
                    float fc = std::clamp(masterSlot.filterCutoff, 0.01f, 0.99f);
                    float q = 1.0f - std::clamp(masterSlot.filterResonance, 0.0f, 0.95f);
                    float fCoeff = 2.0f * sinf(3.14159265f * fc * 0.5f);
                    fCoeff = std::clamp(fCoeff, 0.01f, 0.99f);
                    
                    // Left
                    float hpL = outL - mixerFilters[0].l1 - q * mixerFilters[0].l2;
                    float bpL = fCoeff * hpL + mixerFilters[0].l1;
                    float lpL = fCoeff * bpL + mixerFilters[0].l2;
                    mixerFilters[0].l1 = bpL;
                    mixerFilters[0].l2 = lpL;
                    
                    // Right
                    float hpR = outR - mixerFilters[0].r1 - q * mixerFilters[0].r2;
                    float bpR = fCoeff * hpR + mixerFilters[0].r1;
                    float lpR = fCoeff * bpR + mixerFilters[0].r2;
                    mixerFilters[0].r1 = bpR;
                    mixerFilters[0].r2 = lpR;
                    
                    outL = masterSlot.filterIsHP ? hpL : lpL;
                    outR = masterSlot.filterIsHP ? hpR : lpR;
                }
                
                // Process Master limiter
                if (masterSlot.limiterEnabled) {
                    slotLimiters[0].process(outL, outR);
                }
                
                outL += metronomeVal + prevSampleValL;
                outR += metronomeVal + prevSampleValR;
                
                output[idx]     = std::clamp(outL, -1.0f, 1.0f);
                output[idx + 1] = std::clamp(outR, -1.0f, 1.0f);
            }
        } else {
            for (int f = 0; f < chunkFrames; f++) {
                int tickOffset = (int)(f * ticksPerFrame);
                int currentFrameTick = currentTick + tickOffset;
                
                int currentBeat = currentFrameTick / ppq;
                if (transport->metronomeEnabled.load() && currentBeat != lastMetronomeBeat) {
                    lastMetronomeBeat = currentBeat;
                    int beatsPerBar = project->beatsPerBar;
                    metronomeClickFreq = ((currentBeat * ppq) % (ppq * beatsPerBar) == 0) ? 1200.0f : 800.0f;
                    metronomeClickSampleRemaining = (int)(0.04f * sampleRate);
                    metronomeClickPhase = 0.0f;
                    metronomeClickPhaseStep = (float)(2.0 * 3.14159265358979323846 * metronomeClickFreq / sampleRate);
                    metronomeClickDecayStep = 1.0f / metronomeClickSampleRemaining;
                }
                
                float metronomeVal = 0.0f;
                if (metronomeClickSampleRemaining > 0) {
                    float amp = metronomeClickSampleRemaining * metronomeClickDecayStep;
                    metronomeVal = sinf(metronomeClickPhase) * amp * 0.15f;
                    metronomeClickPhase += metronomeClickPhaseStep;
                    metronomeClickSampleRemaining--;
                }

                // Render preview sample if active (dry bypass)
                float prevSampleValL = 0.0f;
                float prevSampleValR = 0.0f;
                {
                    std::lock_guard<std::mutex> lock(previewSampleMutex);
                    double playPosVal = previewSample.playPos.load();
                    if (playPosVal >= 0.0) {
                        int previewSize = (int)(previewSample.samples.size() / previewSample.numChannels);
                        double ratio = previewSample.sampleRate / (double)sampleRate;
                        
                        double currentPos = playPosVal;
                        size_t idx1 = (size_t)currentPos;
                        size_t idx2 = idx1 + 1;
                        if (idx2 >= (size_t)previewSize) idx2 = idx1;
                        
                        if (idx1 < (size_t)previewSize) {
                            float t = (float)(currentPos - idx1);
                            int chCount = previewSample.numChannels;
                            
                            float s1_L = previewSample.samples[idx1 * chCount];
                            float s2_L = previewSample.samples[idx2 * chCount];
                            prevSampleValL = (s1_L + t * (s2_L - s1_L)) * 0.6f;
                            
                            if (chCount > 1) {
                                float s1_R = previewSample.samples[idx1 * chCount + 1];
                                float s2_R = previewSample.samples[idx2 * chCount + 1];
                                prevSampleValR = (s1_R + t * (s2_R - s1_R)) * 0.6f;
                            } else {
                                prevSampleValR = prevSampleValL;
                            }
                            
                            playPosVal += ratio;
                            if (playPosVal >= previewSize) {
                                playPosVal = -1.0;
                            }
                            previewSample.playPos.store(playPosVal);
                        } else {
                            previewSample.playPos.store(-1.0);
                        }
                    }
                }

                // Process Master slot (0) filter on mono output
                int idx = processedFrames + f;
                float outMono = output[idx] * mv;
                auto& masterSlot = project->mixer[0];
                if (masterSlot.filterEnabled) {
                    float fc = std::clamp(masterSlot.filterCutoff, 0.01f, 0.99f);
                    float q = 1.0f - std::clamp(masterSlot.filterResonance, 0.0f, 0.95f);
                    float fCoeff = 2.0f * sinf(3.14159265f * fc * 0.5f);
                    fCoeff = std::clamp(fCoeff, 0.01f, 0.99f);
                    
                    float hp = outMono - mixerFilters[0].l1 - q * mixerFilters[0].l2;
                    float bp = fCoeff * hp + mixerFilters[0].l1;
                    float lp = fCoeff * bp + mixerFilters[0].l2;
                    mixerFilters[0].l1 = bp;
                    mixerFilters[0].l2 = lp;
                    
                    outMono = masterSlot.filterIsHP ? hp : lp;
                }
                
                // Process Master limiter on mono output
                if (masterSlot.limiterEnabled) {
                    slotLimiters[0].process(outMono, outMono);
                }
                
                float s = outMono + metronomeVal + (prevSampleValL + prevSampleValR) * 0.5f;
                output[idx] = std::clamp(s, -1.0f, 1.0f);
            }
        }

        currentTick += (int)(chunkFrames * ticksPerFrame);
        processedFrames += chunkFrames;
        if (currentTick >= totalTicks) {
            if (transport->looping.load()) {
                currentTick = 0;
                resetAll();
            } else {
                transport->state.store(TransportState::Stopped);
                playHead.store(0);
                transport->playHead.store(0);
                resetAll();
                audioMutex.unlock();
                return;
            }
        }
    }

    playHead.store(currentTick);
    transport->playHead.store(currentTick);
    audioMutex.unlock();
}
