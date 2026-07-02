#include "Project.h"
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <cstring>
static bool writeWAV(const std::string& path, const float* samples, size_t numFrames, int sampleRate, int numChannels) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    uint32_t dataBytes = (uint32_t)(numFrames * numChannels * sizeof(float));
    uint32_t fileSize = 36 + dataBytes;
    auto write32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto write16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    f.write("RIFF", 4);
    write32(fileSize);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    write32(16);
    write16(3);
    write16(numChannels);
    write32(sampleRate);
    write32(sampleRate * numChannels * sizeof(float));
    write16(numChannels * sizeof(float));
    write16(32);
    f.write("data", 4);
    write32(dataBytes);
    f.write((const char*)samples, dataBytes);
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

bool projectImportMIDI(Project& project, const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    auto read16 = [&]() -> uint16_t {
        uint8_t b[2];
        fread(b, 1, 2, f);
        return (uint16_t)b[0] << 8 | b[1];
    };
    auto read32 = [&]() -> uint32_t {
        uint8_t b[4];
        fread(b, 1, 4, f);
        return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    };
    auto readVarLen = [&]() -> int {
        int value = 0;
        uint8_t c;
        do {
            fread(&c, 1, 1, f);
            value = (value << 7) | (c & 0x7F);
        } while (c & 0x80);
        return value;
    };

    char header[4];
    fread(header, 1, 4, f);
    if (memcmp(header, "MThd", 4) != 0) { fclose(f); return false; }
    uint32_t chunkSize = read32();
    (void)chunkSize;
    uint16_t format = read16();
    uint16_t numTracks = read16();
    uint16_t division = read16();
    (void)format;
    int ppq = division & 0x8000 ? 480 : division;
    project.ppq = ppq;
    struct MIDINote {
        int startTick, length, key, velocity, track;
    };
    std::vector<MIDINote> midiNotes;
    for (int t = 0; t < numTracks; t++) {
        char trkHdr[4];
        fread(trkHdr, 1, 4, f);
        if (memcmp(trkHdr, "MTrk", 4) != 0) { fclose(f); return false; }
        uint32_t trkLen = read32();
        long trkStart = ftell(f);
        long trkEnd = trkStart + trkLen;
        int tick = 0;
        uint8_t lastStatus = 0;
        struct PendingNote {
            int key, track, startTick;
        };
        std::vector<PendingNote> pendingNotes;
        while (ftell(f) < trkEnd) {
            int delta = readVarLen();
            tick += delta;
            uint8_t status;
            fread(&status, 1, 1, f);
            if (status < 0x80) {
                fseek(f, -1, SEEK_CUR);
                status = lastStatus;
            }
            lastStatus = status;
            uint8_t type = status & 0xF0;
            if (type == 0x80) {
                uint8_t key, vel;
                fread(&key, 1, 1, f);
                fread(&vel, 1, 1, f);
                (void)vel;
                for (auto& pn : pendingNotes) {
                    if (pn.key == key && pn.track == t) {
                        midiNotes.push_back({pn.startTick, tick - pn.startTick, key, 100, t});
                        pn.key = -1;
                    }
                }
            } else if (type == 0x90) {
                uint8_t key, vel;
                fread(&key, 1, 1, f);
                fread(&vel, 1, 1, f);
                if (vel > 0) {
                    pendingNotes.push_back({key, t, tick});
                } else {
                    for (auto& pn : pendingNotes) {
                        if (pn.key == key && pn.track == t) {
                            midiNotes.push_back({pn.startTick, tick - pn.startTick, key, 100, t});
                            pn.key = -1;
                        }
                    }
                }
            } else if (type == 0xA0) {
                uint8_t a, b;
                fread(&a, 1, 1, f); fread(&b, 1, 1, f);
            } else if (type == 0xB0) {
                uint8_t c, v;
                fread(&c, 1, 1, f); fread(&v, 1, 1, f);
            } else if (type == 0xC0) {
                uint8_t p;
                fread(&p, 1, 1, f);
            } else if (type == 0xD0) {
                uint8_t p;
                fread(&p, 1, 1, f);
            } else if (type == 0xE0) {
                uint8_t lsb, msb;
                fread(&lsb, 1, 1, f); fread(&msb, 1, 1, f);
            } else if (type == 0xF0 || status == 0xFF) {
                if (status == 0xFF) {
                    uint8_t metaType;
                    fread(&metaType, 1, 1, f);
                    int len = readVarLen();
                    fseek(f, len, SEEK_CUR);
                } else {
                    int len = readVarLen();
                    fseek(f, len, SEEK_CUR);
                }
            }
        }

        for (auto& pn : pendingNotes) {
            if (pn.key >= 0 && pn.track == t) {
                midiNotes.push_back({pn.startTick, tick - pn.startTick, pn.key, 100, t});
            }
        }
    }

    fclose(f);
    if (midiNotes.empty()) return false;
    int firstTick = midiNotes[0].startTick;
    if (firstTick > ppq * 2) firstTick = 0;
    for (auto& n : midiNotes) n.startTick -= firstTick;
    int maxTick = 0;
    Pattern pat;
    pat.name = "Imported MIDI";
    pat.lengthTicks = ppq * 16;
    pat.channelIndex = 0;
    for (auto& mn : midiNotes) {
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
    int patIdx = (int)project.patterns.size();
    project.patterns.push_back(pat);
    project.selectedPattern = patIdx;
    Channel ch;
    ch.name = "MIDI Import";
    project.channels.push_back(ch);
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
