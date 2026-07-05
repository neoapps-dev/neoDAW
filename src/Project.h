#pragma once
#include "Types.h"
#include "AudioEngine.h"
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
inline void to_json(json& j, const Note& n) {
    j = json{{"start", n.start}, {"length", n.length}, {"key", n.key}, {"velocity", n.velocity}};
}
inline void from_json(const json& j, Note& n) {
    j.at("start").get_to(n.start);
    j.at("length").get_to(n.length);
    j.at("key").get_to(n.key);
    j.at("velocity").get_to(n.velocity);
}
inline void to_json(json& j, const Channel& c) {
    j = json{{"name", c.name}, {"isSampler", c.isSampler}, {"waveform", (int)c.waveform},
             {"attack", c.attack}, {"decay", c.decay}, {"sustain", c.sustain}, {"release", c.release},
             {"filterCutoff", c.filterCutoff}, {"filterResonance", c.filterResonance},
             {"filterType", (int)c.filterType}, {"samplePath", c.samplePath},
             {"sampleStart", c.sampleStart}, {"sampleEnd", c.sampleEnd},
             {"useSF2", c.useSF2}, {"sf2Path", c.sf2Path}, {"sf2Preset", c.sf2Preset},
             {"volume", c.volume}, {"pan", c.pan}, {"mixerChannel", c.mixerChannel},
             {"muted", c.muted}};
}
inline void from_json(const json& j, Channel& c) {
    j.at("name").get_to(c.name);
    j.at("isSampler").get_to(c.isSampler);
    if (j.contains("waveform")) c.waveform = (Waveform)j.at("waveform").get<int>();
    j.at("attack").get_to(c.attack);
    j.at("decay").get_to(c.decay);
    j.at("sustain").get_to(c.sustain);
    j.at("release").get_to(c.release);
    j.at("filterCutoff").get_to(c.filterCutoff);
    j.at("filterResonance").get_to(c.filterResonance);
    if (j.contains("filterType")) c.filterType = (FilterType)j.at("filterType").get<int>();
    j.at("samplePath").get_to(c.samplePath);
    if (j.contains("sampleStart")) j.at("sampleStart").get_to(c.sampleStart);
    if (j.contains("sampleEnd")) j.at("sampleEnd").get_to(c.sampleEnd);
    if (j.contains("useSF2")) j.at("useSF2").get_to(c.useSF2);
    if (j.contains("sf2Path")) j.at("sf2Path").get_to(c.sf2Path);
    if (j.contains("sf2Preset")) j.at("sf2Preset").get_to(c.sf2Preset);
    j.at("volume").get_to(c.volume);
    j.at("pan").get_to(c.pan);
    j.at("mixerChannel").get_to(c.mixerChannel);
    j.at("muted").get_to(c.muted);
}
inline void to_json(json& j, const Pattern& p) {
    j = json{{"name", p.name}, {"notes", p.notes}, {"color", p.color},
             {"lengthTicks", p.lengthTicks}, {"channelIndex", p.channelIndex}};
}
inline void from_json(const json& j, Pattern& p) {
    j.at("name").get_to(p.name);
    j.at("notes").get_to(p.notes);
    if (j.contains("color")) j.at("color").get_to(p.color);
    j.at("lengthTicks").get_to(p.lengthTicks);
    j.at("channelIndex").get_to(p.channelIndex);
}
inline void to_json(json& j, const MixerSlot& m) {
    j = json{{"name", m.name}, {"volume", m.volume}, {"pan", m.pan}, {"muted", m.muted},
             {"delayEnabled", m.delayEnabled}, {"delayPingPong", m.delayPingPong}, {"delayTime", m.delayTime},
             {"delayFeedback", m.delayFeedback}, {"delayWet", m.delayWet},
             {"filterEnabled", m.filterEnabled}, {"filterIsHP", m.filterIsHP},
             {"filterCutoff", m.filterCutoff}, {"filterResonance", m.filterResonance},
             {"limiterEnabled", m.limiterEnabled},
             {"reverbEnabled", m.reverbEnabled}, {"reverbRoomSize", m.reverbRoomSize},
             {"reverbDamping", m.reverbDamping}, {"reverbWet", m.reverbWet},
             {"chorusEnabled", m.chorusEnabled}, {"chorusRate", m.chorusRate},
             {"chorusDepth", m.chorusDepth}, {"chorusMix", m.chorusMix},
             {"distortionEnabled", m.distortionEnabled}, {"distortionDrive", m.distortionDrive}};
}
inline void from_json(const json& j, MixerSlot& m) {
    j.at("name").get_to(m.name);
    j.at("volume").get_to(m.volume);
    j.at("pan").get_to(m.pan);
    j.at("muted").get_to(m.muted);
    if (j.contains("delayEnabled")) j.at("delayEnabled").get_to(m.delayEnabled);
    if (j.contains("delayPingPong")) j.at("delayPingPong").get_to(m.delayPingPong);
    if (j.contains("delayTime")) j.at("delayTime").get_to(m.delayTime);
    if (j.contains("delayFeedback")) j.at("delayFeedback").get_to(m.delayFeedback);
    if (j.contains("delayWet")) j.at("delayWet").get_to(m.delayWet);
    if (j.contains("filterEnabled")) j.at("filterEnabled").get_to(m.filterEnabled);
    if (j.contains("filterIsHP")) j.at("filterIsHP").get_to(m.filterIsHP);
    if (j.contains("filterCutoff")) j.at("filterCutoff").get_to(m.filterCutoff);
    if (j.contains("filterResonance")) j.at("filterResonance").get_to(m.filterResonance);
    if (j.contains("limiterEnabled")) j.at("limiterEnabled").get_to(m.limiterEnabled);
    if (j.contains("reverbEnabled")) j.at("reverbEnabled").get_to(m.reverbEnabled);
    if (j.contains("reverbRoomSize")) j.at("reverbRoomSize").get_to(m.reverbRoomSize);
    if (j.contains("reverbDamping")) j.at("reverbDamping").get_to(m.reverbDamping);
    if (j.contains("reverbWet")) j.at("reverbWet").get_to(m.reverbWet);
    if (j.contains("chorusEnabled")) j.at("chorusEnabled").get_to(m.chorusEnabled);
    if (j.contains("chorusRate")) j.at("chorusRate").get_to(m.chorusRate);
    if (j.contains("chorusDepth")) j.at("chorusDepth").get_to(m.chorusDepth);
    if (j.contains("chorusMix")) j.at("chorusMix").get_to(m.chorusMix);
    if (j.contains("distortionEnabled")) j.at("distortionEnabled").get_to(m.distortionEnabled);
    if (j.contains("distortionDrive")) j.at("distortionDrive").get_to(m.distortionDrive);
}
inline void to_json(json& j, const PlaylistClip& c) {
    j = json{{"track", c.track}, {"startTick", c.startTick},
             {"patternIndex", c.patternIndex}, {"muted", c.muted}};
}
inline void from_json(const json& j, PlaylistClip& c) {
    j.at("track").get_to(c.track);
    j.at("startTick").get_to(c.startTick);
    j.at("patternIndex").get_to(c.patternIndex);
    j.at("muted").get_to(c.muted);
}

struct ProjectFile {
    static bool save(const Project& project, const std::string& path) {
        json j;
        j["version"] = 1;
        j["name"] = project.name;
        j["bpm"] = project.bpm;
        j["ppq"] = project.ppq;
        j["beatsPerBar"] = project.beatsPerBar;
        j["channels"] = project.channels;
        j["patterns"] = project.patterns;
        j["mixer"] = project.mixer;
        j["playlist"] = project.playlist;
        j["selectedPattern"] = project.selectedPattern;
        j["selectedChannel"] = project.selectedChannel;
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    static bool load(Project& project, const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        json j;
        try {
            file >> j;
        } catch (...) { return false; }
        if (j.contains("version")) {
            project.name = j.value("name", "Untitled");
            project.bpm = j.value("bpm", 130.0f);
            project.ppq = j.value("ppq", 480);
            project.beatsPerBar = j.value("beatsPerBar", 4);
            if (j.contains("channels")) j.at("channels").get_to(project.channels);
            if (j.contains("patterns")) j.at("patterns").get_to(project.patterns);
            if (j.contains("mixer")) j.at("mixer").get_to(project.mixer);
            if (j.contains("playlist")) j.at("playlist").get_to(project.playlist);
            project.selectedPattern = j.value("selectedPattern", -1);
            project.selectedChannel = j.value("selectedChannel", 0);
            project.filePath = path;
        }
        project.modified = false;
        return true;
    }

};

bool projectExportWAV(const Project& project, const std::string& path, int sampleRate = 44100);
bool projectImportMIDI(Project& project, const std::string& path, const std::string& sf2Path = "");
bool projectExportMIDI(const Project& project, const std::string& path);
