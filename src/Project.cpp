#include "Project.h"
#include <MidiFile.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <cmath>
static bool writeWAV(const std::string& path, const float* samples, size_t numFrames, int sampleRate, int numChannels) {
    auto file = juce::File(path);
    file.deleteFile();
    auto outputStream = file.createOutputStream();
    if (!outputStream) return false;
    juce::WavAudioFormat wavFormat;
    auto* writer = wavFormat.createWriterFor(outputStream.release(), (double)sampleRate, numChannels, 24, {}, 0);
    if (!writer) return false;
    juce::AudioBuffer<float> buffer(numChannels, (int)numFrames);
    for (int ch = 0; ch < numChannels; ch++) buffer.copyFrom(ch, 0, samples + ch, numChannels, (int)numFrames);
    writer->writeFromAudioSampleBuffer(buffer, 0, (int)numFrames);
    delete writer;
    return true;
}

bool projectExportWAV(const Project& project, const std::string& path, int sampleRate) {
    int totalTicks = 0;
    for (auto& clip : project.playlist) {
        auto& pat = project.patterns[clip.patternIndex];
        int end = clip.startTick + pat.lengthTicks;
        if (end > totalTicks) totalTicks = end;
    }
    if (totalTicks == 0 && !project.patterns.empty()) {
        totalTicks = project.patterns[0].lengthTicks;
    }
    if (totalTicks <= 0) return false;
    float secPerTick = 60.0f / (project.bpm * project.ppq);
    double totalDuration = totalTicks * secPerTick;
    int numFrames = (int)(totalDuration * sampleRate);
    int numChannels = 2;
    std::vector<float> mixBuffer((size_t)numFrames * numChannels, 0.0f);
    for (size_t ci = 0; ci < project.playlist.size(); ci++) {
        auto& clip = project.playlist[ci];
        if (clip.muted) continue;
        if (clip.patternIndex < 0 || clip.patternIndex >= (int)project.patterns.size()) continue;
        auto& pattern = project.patterns[clip.patternIndex];
        int chanIdx = pattern.channelIndex;
        if (chanIdx < 0 || chanIdx >= (int)project.channels.size()) continue;
        auto& ch = project.channels[chanIdx];
        if (ch.muted) continue;
        Synthesizer synth;
        EnvelopeParams env{ch.attack, ch.decay, ch.sustain, ch.release};
        synth.setEnvelope(env);
        synth.waveform = ch.waveform;
        synth.filterCutoff = ch.filterCutoff;
        synth.filterResonance = ch.filterResonance;
        synth.filterType = ch.filterType;
        synth.volume = ch.volume;
        synth.pan = ch.pan;
        int patStartTick = clip.startTick;
        int patLen = pattern.lengthTicks;
        int patEndTick = patStartTick + patLen;
        int startFrame = (int)(patStartTick * secPerTick * sampleRate);
        int patFrames = (int)(patLen * secPerTick * sampleRate);
        if (startFrame + patFrames > numFrames) patFrames = numFrames - startFrame;
        if (patFrames <= 0) continue;
        std::vector<float> patBuf((size_t)patFrames * numChannels, 0.0f);
        int currentTick = 0;
        int processedFrames = 0;
        std::vector<int> activeKeys;
        float ticksPerFrame = project.bpm * project.ppq / (60.0f * sampleRate);
        while (processedFrames < patFrames) {
            int chunkFrames = std::min(patFrames - processedFrames, 512);
            float temp[1024];
            std::fill(temp, temp + chunkFrames * numChannels, 0.0f);
            std::vector<int> toTurnOff;
            std::vector<int> toTurnOn;
            for (auto& note : pattern.notes) {
                int noteEnd = note.start + note.length;
                if (note.start <= currentTick && noteEnd > currentTick) {
                    if (std::find(activeKeys.begin(), activeKeys.end(), note.key) == activeKeys.end()) {
                        toTurnOn.push_back(note.key);
                    }
                } else if (noteEnd <= currentTick) {
                    toTurnOff.push_back(note.key);
                }
            }

            for (int k : toTurnOn) {
                activeKeys.push_back(k);
                synth.noteOn(k, 100 / 127.0f, currentTick);
            }
            for (int k : toTurnOff) {
                auto it = std::find(activeKeys.begin(), activeKeys.end(), k);
                if (it != activeKeys.end()) {
                    activeKeys.erase(it);
                    synth.noteOff(k, currentTick);
                }
            }

            synth.render(temp, chunkFrames, numChannels, (float)sampleRate, currentTick, patLen, project.bpm, project.ppq);
            for (int f = 0; f < chunkFrames * numChannels; f++) patBuf[processedFrames * numChannels + f] += temp[f];
            currentTick += (int)(chunkFrames * ticksPerFrame);
            processedFrames += chunkFrames;
        }

        synth.allNotesOff();
        for (size_t f = 0; f < (size_t)patFrames * numChannels; f++) mixBuffer[startFrame * numChannels + f] += patBuf[f];
    }

    float maxVal = 0.0f;
    for (auto& s : mixBuffer) {
        float absVal = std::fabs(s);
        if (absVal > maxVal) maxVal = absVal;
    }
    if (maxVal > 1.0f) {
        float gain = 0.9f / maxVal;
        for (auto& s : mixBuffer) s *= gain;
    }

    return writeWAV(path, mixBuffer.data(), numFrames, sampleRate, numChannels);
}

bool projectImportMIDI(Project& project, const std::string& path, const std::string& sf2Path) {
    smf::MidiFile midifile;
    if (!midifile.read(path)) return false;
    midifile.linkNotePairs();
    int ppq = midifile.getTicksPerQuarterNote();
    if (ppq <= 0) ppq = 480;
    project.ppq = ppq;
    std::vector<int> savedProgram;
    int channelProgram[16];
    std::fill(channelProgram, channelProgram + 16, -1);
    for (int t = 0; t < midifile.getTrackCount(); t++) {
        for (int e = 0; e < midifile[t].size(); e++) {
            auto& ev = midifile[t][e];
            if (ev.isPatchChange()) {
                int ch = ev.getChannel();
                if (ch >= 0 && ch < 16 && ev.getP1() >= 0 && ev.getP1() < 128)
                    channelProgram[ch] = ev.getP1();
            }
        }
    }

    struct MIDINote {
        int startTick, length, key, velocity, channel;
    };
    std::vector<MIDINote> midiNotes;
    for (int t = 0; t < midifile.getTrackCount(); t++) {
        for (int e = 0; e < midifile[t].size(); e++) {
            auto& ev = midifile[t][e];
            if (ev.isNoteOn()) {
                int ch = ev.getChannel();
                if (ch < 0 || ch >= 16) ch = 0;
                int dur = ev.getTickDuration();
                if (dur < ppq / 4) dur = ppq / 4;
                midiNotes.push_back({ev.tick, dur, ev.getKeyNumber(), ev.getVelocity(), ch});
            }
        }
    }

    if (midiNotes.empty()) return false;
    int firstTick = midiNotes[0].startTick;
    if (firstTick > ppq * 2) firstTick = 0;
    std::map<int, std::vector<MIDINote>> notesByChannel;
    for (auto& mn : midiNotes) {
        mn.startTick = std::max(0, mn.startTick - firstTick);
        notesByChannel[mn.channel].push_back(mn);
    }

    for (auto& [chNum, chNotes] : notesByChannel) {
        Pattern pat;
        pat.name = sf2Path.empty() ? "Ch." + std::to_string(chNum + 1) : "Ch." + std::to_string(chNum + 1) + " [" + sf2Path.substr(sf2Path.find_last_of("/\\") + 1) + "]";
        pat.lengthTicks = ppq * 16;
        pat.channelIndex = (int)project.channels.size();
        int maxTick = 0;
        for (auto& mn : chNotes) {
            Note n;
            n.start = std::max(0, mn.startTick);
            n.length = std::max(mn.length, ppq / 4);
            n.key = mn.key;
            n.velocity = mn.velocity;
            pat.notes.push_back(n);
            if (n.start + n.length > maxTick) maxTick = n.start + n.length;
        }

        pat.lengthTicks = ((maxTick / ppq) + 2) * ppq;
        if (pat.lengthTicks < ppq * 4) pat.lengthTicks = ppq * 4;
        project.patterns.push_back(pat);
        Channel ch;
        ch.name = "Ch." + std::to_string(chNum + 1);
        if (!sf2Path.empty()) {
            ch.useSF2 = true;
            ch.sf2Path = sf2Path;
            int prog = channelProgram[chNum];
            if (prog >= 0) ch.sf2Preset = prog;
            ch.name += " [" + sf2Path.substr(sf2Path.find_last_of("/\\") + 1) + "]";
        }
        project.channels.push_back(ch);
    }

    if (!project.channels.empty())
        project.selectedChannel = (int)project.channels.size() - 1;
    if (!project.patterns.empty())
        project.selectedPattern = (int)project.patterns.size() - 1;
    project.modified = true;
    return true;
}

static void writeVarLen(std::vector<uint8_t>& out, int value) {
    uint8_t buf[4];
    int i = 0;
    buf[i++] = value & 0x7F;
    while ((value >>= 7) > 0) buf[i++] = (value & 0x7F) | 0x80;
    while (i > 0) out.push_back(buf[--i]);
}

bool projectExportMIDI(const Project& project, const std::string& path) {
    struct SortNote { int tick; int key; int velocity; bool on; int channel; };
    std::vector<SortNote> events;
    for (auto& pat : project.patterns) {
        for (auto& n : pat.notes) {
            events.push_back({n.start, n.key, n.velocity, true, pat.channelIndex});
            events.push_back({n.start + n.length, n.key, 0, false, pat.channelIndex});
        }
    }
    std::sort(events.begin(), events.end(), [](auto& a, auto& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.on < b.on;
    });

    int ppq = project.ppq;
    std::vector<uint8_t> trackData;
    int lastTick = 0;
    for (auto& e : events) {
        int delta = std::max(0, e.tick - lastTick);
        writeVarLen(trackData, delta);
        lastTick = e.tick;
        if (e.on) {
            trackData.push_back(0x90);
            trackData.push_back((uint8_t)std::clamp(e.key, 0, 127));
            trackData.push_back((uint8_t)std::clamp(e.velocity, 1, 127));
        } else {
            trackData.push_back(0x80);
            trackData.push_back((uint8_t)std::clamp(e.key, 0, 127));
            trackData.push_back(0x40);
        }
    }
    writeVarLen(trackData, 0);
    trackData.push_back(0xFF);
    trackData.push_back(0x2F);
    trackData.push_back(0x00);
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    auto write16 = [&](uint16_t v) { uint16_t be = (v >> 8) | (v << 8); f.write((const char*)&be, 2); };
    auto write32 = [&](uint32_t v) {
        uint32_t be = ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
        f.write((const char*)&be, 4);
    };
    f.write("MThd", 4);
    write32(6);
    write16(0);
    write16(1);
    write16(ppq);
    uint32_t trackLen = (uint32_t)trackData.size();
    f.write("MTrk", 4);
    write32(trackLen);
    f.write((const char*)trackData.data(), trackData.size());
    return true;
}
