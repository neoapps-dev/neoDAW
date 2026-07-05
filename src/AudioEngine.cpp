#include "AudioEngine.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
bool AudioEngine::loadWavFile(const std::string& path, SampleData& sd) {
    auto file = juce::File(path);
    if (!file.existsAsFile()) return false;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));
    if (!reader) return false;
    sd.sampleRate = (int)reader->sampleRate;
    sd.numChannels = (int)reader->numChannels;
    int numFrames = (int)reader->lengthInSamples;
    sd.samples.resize((size_t)numFrames * sd.numChannels);
    juce::AudioBuffer<float> tempBuf(sd.numChannels, numFrames);
    reader->read(&tempBuf, 0, numFrames, 0, true, true);
    for (int ch = 0; ch < sd.numChannels; ch++) for (int s = 0; s < numFrames; s++) sd.samples[(size_t)s * sd.numChannels + ch] = tempBuf.getSample(ch, s);
    sd.loaded = true;
    return true;
}

bool AudioEngine::writeWavToFile(const std::string& path, const float* interleavedData, int numFrames, int numChannels, int sampleRate) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    int bitsPerSample = 24;
    int bytesPerSample = bitsPerSample / 8;
    int dataSize = numFrames * numChannels * bytesPerSample;
    int chunkSize = 36 + dataSize;
    uint32_t hdr[11];
    memcpy(hdr, "RIFF", 4); hdr[1] = (uint32_t)chunkSize;
    memcpy(&hdr[2], "WAVEfmt ", 8); hdr[4] = 16; hdr[5] = (uint32_t)(1 | (numChannels << 16));
    hdr[6] = (uint32_t)sampleRate;
    hdr[7] = (uint32_t)(sampleRate * numChannels * bytesPerSample);
    hdr[8] = (uint32_t)((numChannels * bytesPerSample) | (bitsPerSample << 16));
    memcpy(&hdr[9], "data", 4); hdr[10] = (uint32_t)dataSize;
    fwrite(hdr, 44, 1, f);
    std::vector<uint8_t> buf(65536 * 3);
    size_t bufPos = 0;
    for (int i = 0; i < numFrames * numChannels; i++) {
        int s = (int)(interleavedData[i] * 8388607.0f);
        if (s > 8388607) s = 8388607;
        else if (s < -8388608) s = -8388608;
        buf[bufPos++] = (uint8_t)(s & 0xFF);
        buf[bufPos++] = (uint8_t)((s >> 8) & 0xFF);
        buf[bufPos++] = (uint8_t)((s >> 16) & 0xFF);
        if (bufPos >= buf.size()) { fwrite(buf.data(), 1, bufPos, f); bufPos = 0; }
    }
    if (bufPos > 0) fwrite(buf.data(), 1, bufPos, f);
    fclose(f);
    return true;
}

AudioEngine::AudioEngine() {}
AudioEngine::~AudioEngine() { shutdown(); }
bool AudioEngine::init(const AudioSettings& settings) {
    if (initialized) return true;
    sampleRate = settings.sampleRate;
    numChannels = settings.numChannels;
    audioDeviceManager = std::make_unique<juce::AudioDeviceManager>();
    juce::String error = audioDeviceManager->initialise(0, numChannels, nullptr, true);
    if (error.isNotEmpty()) {
        fprintf(stderr, "JUCE audio init failed: %s\n", error.toRawUTF8());
        audioDeviceManager.reset();
        return false;
    }
    fluidSettings = new_fluid_settings();
    if (fluidSettings) {
        fluid_set_log_function(FLUID_WARN, NULL, NULL);
        fluid_set_log_function(FLUID_INFO, NULL, NULL);
        fluidSynth = new_fluid_synth(fluidSettings);
    }
    audioDeviceManager->addAudioCallback(this);
    initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (fluidSynth) { delete_fluid_synth(fluidSynth); fluidSynth = nullptr; }
    if (fluidSettings) { delete_fluid_settings(fluidSettings); fluidSettings = nullptr; }
    fluidSfontId = -1;
    if (audioDeviceManager) {
        audioDeviceManager->removeAudioCallback(this);
        audioDeviceManager->closeAudioDevice();
        audioDeviceManager.reset();
    }
    initialized = false;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    if (device) {
        sampleRate = (int)device->getCurrentSampleRate();
    }
}

void AudioEngine::audioDeviceStopped() {}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&) {
    if (numOutputChannels < 1 || numSamples <= 0) return;
    size_t totalSamples = (size_t)numSamples * (size_t)numOutputChannels;
    std::vector<float> temp(totalSamples, 0.0f);
    render(temp.data(), (unsigned int)numSamples);
    for (int ch = 0; ch < numOutputChannels; ch++) {
        int c = std::min(ch, numChannels - 1);
        for (int s = 0; s < numSamples; s++) outputChannelData[ch][s] = temp[(size_t)s * numOutputChannels + c];
    }
    if (previewMutex.try_lock()) {
        float previewBuf[4096];
        int prevFrames = std::min(numSamples, (int)(sizeof(previewBuf) / sizeof(float) / numOutputChannels));
        if (prevFrames > 0) {
            std::memset(previewBuf, 0, (size_t)prevFrames * numOutputChannels * sizeof(float));
            previewSynth.render(previewBuf, prevFrames, numOutputChannels, (float)sampleRate, 0, 999999, 120, 480);
            for (int ch = 0; ch < numOutputChannels; ch++) {
                int c = std::min(ch, numChannels - 1);
                for (int s = 0; s < prevFrames; s++) outputChannelData[ch][s] += previewBuf[(size_t)s * numOutputChannels + c];
            }
        }
        previewMutex.unlock();
    }
}

void AudioEngine::startPlayback() {
    if (!initialized || !transport) return;
    for (auto& s : synthPool) s.forceReset();
    activeNotes.clear();
    clipRenderStates.clear();
    playHead.store(0);
    tickRemainder = 0.0;
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
    if (slotReverbsInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotReverbs[i].reset();
    }
    if (slotChorusesInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotChoruses[i].reset();
    }
    transport->state.store(TransportState::Playing);
}

void AudioEngine::stopPlayback() {
    if (!transport) return;
    transport->state.store(TransportState::Stopped);
    playHead.store(0);
    tickRemainder = 0.0;
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
    if (slotReverbsInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotReverbs[i].reset();
    }
    if (slotChorusesInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotChoruses[i].reset();
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
            tickRemainder = 0.0;
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
    bool wasPlaying = audioDeviceManager && audioDeviceManager->getCurrentAudioDevice() != nullptr;
    if (wasPlaying) audioDeviceManager->removeAudioCallback(this);
    const int chunkSize = 512;
    while (frameIdx < numFrames) {
        int todo = std::min(chunkSize, numFrames - frameIdx);
        render(mix.data() + (size_t)frameIdx * numChannels, (unsigned int)todo);
        frameIdx += todo;
    }
    if (wasPlaying) audioDeviceManager->addAudioCallback(this);
    transport->state.store(TransportState::Stopped);
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }
    return writeWavToFile(path, mix.data(), numFrames, numChannels, sampleRate);
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
    playHead.store(0);
    tickRemainder = 0.0;
    activeNotes.clear();
    bpm.store(proj.bpm);
    bool wasPlaying = audioDeviceManager && audioDeviceManager->getCurrentAudioDevice() != nullptr;
    if (wasPlaying) audioDeviceManager->removeAudioCallback(this);
    const int chunkSize = 512;
    while (frameIdx < numFrames) {
        int todo = std::min(chunkSize, numFrames - frameIdx);
        render(mix.data() + (size_t)frameIdx * numChannels, (unsigned int)todo);
        frameIdx += todo;
    }
    if (wasPlaying) audioDeviceManager->addAudioCallback(this);
    transport->state.store(TransportState::Stopped);
    sampleRate = saveSampleRate;
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }
    return writeWavToFile(path, mix.data(), numFrames, numChannels, outSampleRate);
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
    if (proj.patterns.empty()) return false;
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
    int processedFrames = 0;
    double preciseTick = 0.0;
    int currentTick = 0;
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
        currentTick = (int)preciseTick;
        processedFrames += chunkFrames;
    }

    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    return writeWavToFile(path, mix.data(), numFrames, numChannels, sampleRate);
}

bool AudioEngine::debugRenderPoolFreshLoop(const Project& proj, const Transport& trans, const char* path) {
    int totalTicks = 0;
    for (auto& p : proj.patterns) if (p.lengthTicks > totalTicks) totalTicks = p.lengthTicks;
    if (totalTicks <= 0) totalTicks = 1920;
    float secPerTick = 60.0f / (proj.bpm * proj.ppq);
    double totalSec = totalTicks * secPerTick;
    int numFrames = (int)(totalSec * sampleRate);
    if (numFrames <= 0) return false;
    bool wasPlaying = audioDeviceManager && audioDeviceManager->getCurrentAudioDevice() != nullptr;
    if (wasPlaying) audioDeviceManager->removeAudioCallback(this);
    std::vector<float> mix((size_t)numFrames * numChannels, 0.0f);
    for (auto& s : synthPool) s.forceReset();
    if (proj.patterns.empty()) { if (wasPlaying) audioDeviceManager->addAudioCallback(this); return false; }
    int selPat = proj.selectedPattern;
    if (selPat < 0 || selPat >= (int)proj.patterns.size()) selPat = 0;
    auto& pattern = proj.patterns[selPat];
    int chanIdx = pattern.channelIndex;
    if (chanIdx < 0 || chanIdx >= 16) { if (wasPlaying) audioDeviceManager->addAudioCallback(this); return false; }
    auto& ch = proj.channels[chanIdx];
    if (ch.muted) { if (wasPlaying) audioDeviceManager->addAudioCallback(this); return false; }
    EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
    synthPool[chanIdx].setEnvelope(env);
    synthPool[chanIdx].waveform = ch.waveform;
    synthPool[chanIdx].filterCutoff = ch.filterCutoff;
    synthPool[chanIdx].filterResonance = ch.filterResonance;
    synthPool[chanIdx].filterType = ch.filterType;
    synthPool[chanIdx].volume = ch.volume;
    synthPool[chanIdx].pan = ch.pan;
    int processedFrames = 0;
    double preciseTick = 0.0;
    int currentTick = 0;
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
        preciseTick += chunkFrames * ticksPerFrame;
        currentTick = (int)preciseTick;
        processedFrames += chunkFrames;
    }

    if (wasPlaying) audioDeviceManager->addAudioCallback(this);
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    return writeWavToFile(path, mix.data(), numFrames, numChannels, sampleRate);
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
    bool wasPlaying = audioDeviceManager && audioDeviceManager->getCurrentAudioDevice() != nullptr;
    if (wasPlaying) audioDeviceManager->removeAudioCallback(this);
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

    if (wasPlaying) audioDeviceManager->addAudioCallback(this);
    transport->state.store(TransportState::Stopped);
    project = saveProject;
    transport = saveTransport;
    for (auto& s : mix) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }

    return writeWavToFile(path, mix.data(), numFrames, numChannels, sampleRate);
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
    double preciseTick = (double)currentTick + tickRemainder;
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
    if (!mixerDelaysInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerDelays[i].init(MAX_DELAY_SAMPLES);
        mixerDelaysInit = true;
    }

    if (!mixerFiltersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            mixerFilters[i].clear();
        mixerFiltersInit = true;
    }

    if (!slotLimitersInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++)
            slotLimiters[i].reset();
        slotLimitersInit = true;
    }

    if (!slotReverbsInit) {
        for (int i = 0; i < NUM_MIXER_SLOTS; i++) slotReverbs[i].reset();
        slotReverbsInit = true;
    }

    if (!slotChorusesInit) {
        juce::dsp::ProcessSpec spec{ (double)sampleRate, 512, 2 };
        for (int i = 0; i < NUM_MIXER_SLOTS; i++) slotChoruses[i].prepare(spec);
        slotChorusesInit = true;
    }

    bool usePlaylist = hasPlaylist;
    int totalTicks;
    if (usePlaylist) {
        totalTicks = ppq * 4;
        for (auto& clip : project->playlist) {
            if (clip.patternIndex >= 0 && clip.patternIndex < (int)project->patterns.size()) {
                int end = clip.startTick + project->patterns[clip.patternIndex].lengthTicks;
                if (end > totalTicks) totalTicks = end;
            }
        }
    } else {
        int selPat = project->selectedPattern;
        if (selPat < 0 || selPat >= (int)project->patterns.size()) {
            audioMutex.unlock();
            return;
        }
        totalTicks = project->patterns[selPat].lengthTicks;
        if (totalTicks <= 0) {
            audioMutex.unlock();
            return;
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
        for (auto& s : synthPool) s.allNotesOff();
        if (fluidSynth) {
            for (int c = 0; c < 16; c++) {
                fluid_synth_cc(fluidSynth, c, 120, 0);
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
        if (usePlaylist) {
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
                                size_t i0 = startFrame + (size_t)pos;
                                size_t i1 = i0 + 1;
                                if (i0 >= endFrame) i0 = startFrame;
                                if (i1 >= endFrame) i1 = startFrame;
                                double frac = pos - (double)(size_t)pos;
                                size_t sdi0 = i0 * sdChan;
                                size_t sdi1 = i1 * sdChan;
                                float s0 = sd.samples[sdi0];
                                float s1 = sd.samples[sdi1];
                                float sv = (s0 + (float)frac * (s1 - s0)) * ch.volume * mixVol;
                                output[processedFrames * numChannels + f * numChannels] += sv;
                                if (numChannels > 1) {
                                    float sr0 = sdChan > 1 ? sd.samples[sdi0 + 1] : s0;
                                    float sr1 = sdChan > 1 ? sd.samples[sdi1 + 1] : s1;
                                    float sr = (sr0 + (float)frac * (sr1 - sr0)) * ch.volume * mixVol;
                                    output[processedFrames * numChannels + f * numChannels + 1] += sr;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            int selPat = project->selectedPattern;
            if (selPat >= 0 && selPat < (int)project->patterns.size()) {
                auto& pat = project->patterns[selPat];
                int patLen = pat.lengthTicks;
                if (patLen > 0) {
                    int localTick = currentTick;
                    int chanIdx = pat.channelIndex;
                    if (chanIdx >= 0 && chanIdx < 16) {
                        Channel& ch = project->getOrCreateChannel(chanIdx);
                        float mixVol = 1.0f;
                        bool mixMuted = false;
                        if (ch.mixerChannel >= 0 && ch.mixerChannel < (int)project->mixer.size()) {
                            mixVol = project->mixer[ch.mixerChannel].volume;
                            mixMuted = project->mixer[ch.mixerChannel].muted;
                        }
                        if (!ch.muted && !mixMuted) {
                            std::vector<int> toTurnOn, toTurnOff;
                            for (auto& note : pat.notes) {
                                int noteEnd = note.start + note.length;
                                if (note.start <= localTick && noteEnd > localTick) {
                                    if (std::find(activeNotes.begin(), activeNotes.end(), note.key) == activeNotes.end()) toTurnOn.push_back(note.key);
                                } else if (noteEnd <= localTick) {
                                    toTurnOff.push_back(note.key);
                                }
                            }

                            if (ch.useSF2) {
                                for (int k : toTurnOff) {
                                    auto it = std::find(activeNotes.begin(), activeNotes.end(), k);
                                    if (it != activeNotes.end()) {
                                        activeNotes.erase(it);
                                        if (fluidSynth) fluid_synth_noteoff(fluidSynth, chanIdx, k);
                                    }
                                }
                                for (int k : toTurnOn) {
                                    if (std::find(activeNotes.begin(), activeNotes.end(), k) == activeNotes.end()) {
                                        activeNotes.push_back(k);
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
                                    auto it = std::find(activeNotes.begin(), activeNotes.end(), k);
                                    if (it != activeNotes.end()) {
                                        activeNotes.erase(it);
                                        synthPool[chanIdx].noteOff(k, localTick);
                                    }
                                }
                                for (int k : toTurnOn) {
                                    if (std::find(activeNotes.begin(), activeNotes.end(), k) == activeNotes.end()) {
                                        activeNotes.push_back(k);
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
                                            size_t i0 = startFrame + (size_t)pos;
                                            size_t i1 = i0 + 1;
                                            if (i0 >= endFrame) i0 = startFrame;
                                            if (i1 >= endFrame) i1 = startFrame;
                                            double frac = pos - (double)(size_t)pos;
                                            size_t sdi0 = i0 * sdChan;
                                            size_t sdi1 = i1 * sdChan;
                                            float s0 = sd.samples[sdi0];
                                            float s1 = sd.samples[sdi1];
                                            float sv = (s0 + (float)frac * (s1 - s0)) * ch.volume * mixVol;
                                            output[processedFrames * numChannels + f * numChannels] += sv;
                                            if (numChannels > 1) {
                                                float sr0 = sdChan > 1 ? sd.samples[sdi0 + 1] : s0;
                                                float sr1 = sdChan > 1 ? sd.samples[sdi1 + 1] : s1;
                                                float sr = (sr0 + (float)frac * (sr1 - sr0)) * ch.volume * mixVol;
                                                output[processedFrames * numChannels + f * numChannels + 1] += sr;
                                            }
                                        }
                                    }
                                }
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
            for (int f = 0; f < fluidSamples; f++) output[processedFrames * numChannels + f] += fluidBuf[f];
        }

        if (numChannels >= 2) {
            float fxBufL[NUM_MIXER_SLOTS][512];
            float fxBufR[NUM_MIXER_SLOTS][512];
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
                for (int mi = 1; mi < (int)project->mixer.size() && mi < NUM_MIXER_SLOTS; mi++) {
                    auto& slot = project->mixer[mi];
                    float slotSigL = dryL;
                    float slotSigR = dryR;
                    if (slot.distortionEnabled) {
                        float d = slot.distortionDrive;
                        if (d > 0.01f) {
                            slotSigL = std::tanh(slotSigL * (1.0f + d * 4.0f)) / (1.0f + d * 4.0f) * (1.0f + d * 4.0f);
                            slotSigR = std::tanh(slotSigR * (1.0f + d * 4.0f)) / (1.0f + d * 4.0f) * (1.0f + d * 4.0f);
                        }
                    }
                    if (slot.filterEnabled) {
                        float fc = std::clamp(slot.filterCutoff, 0.01f, 0.99f);
                        float q = 1.0f - std::clamp(slot.filterResonance, 0.0f, 0.95f);
                        float fCoeff = 2.0f * sinf(3.14159265f * fc * 0.5f);
                        fCoeff = std::clamp(fCoeff, 0.01f, 0.99f);
                        float hpL = slotSigL - mixerFilters[mi].l1 - q * mixerFilters[mi].l2;
                        float bpL = fCoeff * hpL + mixerFilters[mi].l1;
                        float lpL = fCoeff * bpL + mixerFilters[mi].l2;
                        mixerFilters[mi].l1 = bpL;
                        mixerFilters[mi].l2 = lpL;
                        float hpR = slotSigR - mixerFilters[mi].r1 - q * mixerFilters[mi].r2;
                        float bpR = fCoeff * hpR + mixerFilters[mi].r1;
                        float lpR = fCoeff * bpR + mixerFilters[mi].r2;
                        mixerFilters[mi].r1 = bpR;
                        mixerFilters[mi].r2 = lpR;

                        slotSigL = slot.filterIsHP ? hpL : lpL;
                        slotSigR = slot.filterIsHP ? hpR : lpR;
                    }

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

                    fxBufL[mi][f] = slotSigL;
                    fxBufR[mi][f] = slotSigR;
                }

                float outL = (dryL + wetL) * mv;
                float outR = (dryR + wetR) * mv;
                auto& masterSlot = project->mixer[0];
                if (masterSlot.filterEnabled) {
                    float fc = std::clamp(masterSlot.filterCutoff, 0.01f, 0.99f);
                    float q = 1.0f - std::clamp(masterSlot.filterResonance, 0.0f, 0.95f);
                    float fCoeff = 2.0f * sinf(3.14159265f * fc * 0.5f);
                    fCoeff = std::clamp(fCoeff, 0.01f, 0.99f);
                    float hpL = outL - mixerFilters[0].l1 - q * mixerFilters[0].l2;
                    float bpL = fCoeff * hpL + mixerFilters[0].l1;
                    float lpL = fCoeff * bpL + mixerFilters[0].l2;
                    mixerFilters[0].l1 = bpL;
                    mixerFilters[0].l2 = lpL;
                    float hpR = outR - mixerFilters[0].r1 - q * mixerFilters[0].r2;
                    float bpR = fCoeff * hpR + mixerFilters[0].r1;
                    float lpR = fCoeff * bpR + mixerFilters[0].r2;
                    mixerFilters[0].r1 = bpR;
                    mixerFilters[0].r2 = lpR;
                    outL = masterSlot.filterIsHP ? hpL : lpL;
                    outR = masterSlot.filterIsHP ? hpR : lpR;
                }

                if (masterSlot.limiterEnabled) {
                    slotLimiters[0].process(outL, outR);
                }

                outL += metronomeVal + prevSampleValL;
                outR += metronomeVal + prevSampleValR;
                output[idx]     = outL;
                output[idx + 1] = outR;
            }

            for (int mi = 1; mi < (int)project->mixer.size() && mi < NUM_MIXER_SLOTS; mi++) {
                auto& slot = project->mixer[mi];
                if (slot.chorusEnabled) {
                    slotChoruses[mi].setRate(slot.chorusRate);
                    slotChoruses[mi].setDepth(slot.chorusDepth);
                    slotChoruses[mi].setMix(1.0f);
                    float* chans[2] = { fxBufL[mi], fxBufR[mi] };
                    auto block = juce::dsp::AudioBlock<float>(chans, 2, (size_t)chunkFrames);
                    juce::dsp::ProcessContextReplacing<float> ctx(block);
                    slotChoruses[mi].process(ctx);
                    float wet = slot.chorusMix;
                    for (int f = 0; f < chunkFrames; f++) {
                        int idx = (processedFrames + f) * numChannels;
                        output[idx]     += fxBufL[mi][f] * wet;
                        output[idx + 1] += fxBufR[mi][f] * wet;
                    }
                }
                if (slot.reverbEnabled) {
                    float dryL[512], dryR[512];
                    memcpy(dryL, fxBufL[mi], chunkFrames * sizeof(float));
                    memcpy(dryR, fxBufR[mi], chunkFrames * sizeof(float));
                    juce::dsp::Reverb::Parameters params;
                    params.roomSize = slot.reverbRoomSize;
                    params.damping = slot.reverbDamping;
                    params.wetLevel = 1.0f;
                    params.dryLevel = 0.0f;
                    params.width = 0.8f;
                    params.freezeMode = 0.0f;
                    slotReverbs[mi].setParameters(params);
                    float* chans[2] = { fxBufL[mi], fxBufR[mi] };
                    auto block = juce::dsp::AudioBlock<float>(chans, 2, (size_t)chunkFrames);
                    juce::dsp::ProcessContextReplacing<float> ctx(block);
                    slotReverbs[mi].process(ctx);
                    float wet = slot.reverbWet;
                    for (int f = 0; f < chunkFrames; f++) {
                        int idx = (processedFrames + f) * numChannels;
                        output[idx]     += (fxBufL[mi][f] - dryL[f]) * wet;
                        output[idx + 1] += (fxBufR[mi][f] - dryR[f]) * wet;
                    }
                }
            }

            for (int f = 0; f < chunkFrames; f++) {
                int idx = (processedFrames + f) * numChannels;
                output[idx]     = std::clamp(output[idx], -1.0f, 1.0f);
                output[idx + 1] = std::clamp(output[idx + 1], -1.0f, 1.0f);
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

                if (masterSlot.limiterEnabled) {
                    slotLimiters[0].process(outMono, outMono);
                }

                float s = outMono + metronomeVal + (prevSampleValL + prevSampleValR) * 0.5f;
                output[idx] = std::clamp(s, -1.0f, 1.0f);
            }
        }

        preciseTick += chunkFrames * ticksPerFrame;
        currentTick = (int)preciseTick;
        processedFrames += chunkFrames;
        if (currentTick >= totalTicks) {
            if (transport->looping.load()) {
                currentTick = 0;
                preciseTick = 0.0;
                resetAll();
            } else {
                transport->state.store(TransportState::Stopped);
                playHead.store(0);
                tickRemainder = 0.0;
                transport->playHead.store(0);
                resetAll();
                audioMutex.unlock();
                return;
            }
        }
    }

    tickRemainder = preciseTick - (double)currentTick;
    playHead.store(currentTick);
    transport->playHead.store(currentTick);
    audioMutex.unlock();
}
