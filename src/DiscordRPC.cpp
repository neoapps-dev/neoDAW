#include "DiscordRPC.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/poll.h>
#include <cerrno>
#endif
using json = nlohmann::json;
static std::string getIPCPath() {
#ifdef _WIN32
    return "\\\\.\\pipe\\discord-ipc-0";
#else
    const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDir && runtimeDir[0]) {
        return std::string(runtimeDir) + "/discord-ipc-0";
    }
    const char* tmpDir = std::getenv("TMPDIR");
    if (!tmpDir || !tmpDir[0]) tmpDir = "/tmp";
    return std::string(tmpDir) + "/discord-ipc-0";
#endif
}

DiscordRPCManager::DiscordRPCManager() = default;
DiscordRPCManager::~DiscordRPCManager() {
    shutdown();
}

bool DiscordRPCManager::connectToDiscord() {
    disconnect();
    std::string path = getIPCPath();
    fprintf(stderr, "[DiscordRPC] Connecting to %s\n", path.c_str());
#ifdef _WIN32
    HANDLE pipe = CreateFileA(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) { fprintf(stderr, "[DiscordRPC] CreateFile failed\n"); return false; }
    m_sock = (int)(intptr_t)pipe;
    fprintf(stderr, "[DiscordRPC] Connected via pipe\n");
    return true;
#else
    m_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_sock < 0) { fprintf(stderr, "[DiscordRPC] socket() failed: %s\n", strerror(errno)); return false; }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t pathLen = path.size();
    if (pathLen >= sizeof(addr.sun_path)) pathLen = sizeof(addr.sun_path) - 1;
    memcpy(addr.sun_path, path.data(), pathLen);
    if (connect(m_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[DiscordRPC] connect() failed: %s\n", strerror(errno));
        disconnect();
        return false;
    }
    fprintf(stderr, "[DiscordRPC] Socket connected\n");
    return true;
#endif
}

void DiscordRPCManager::disconnect() {
    if (m_sock < 0) return;
#ifdef _WIN32
    CloseHandle((HANDLE)(intptr_t)m_sock);
#else
    close(m_sock);
#endif
    m_sock = -1;
}

bool DiscordRPCManager::sendFrame(uint32_t opcode, const std::string& data) {
    if (m_sock < 0) { fprintf(stderr, "[DiscordRPC] sendFrame: not connected\n"); return false; }
    uint32_t len = static_cast<uint32_t>(data.size());
    fprintf(stderr, "[DiscordRPC] sendFrame op=%u len=%u data=%.200s\n", opcode, len, data.c_str());
#ifdef _WIN32
    DWORD written;
    HANDLE pipe = (HANDLE)(intptr_t)m_sock;
    if (!WriteFile(pipe, &opcode, 4, &written, NULL) || written != 4) { fprintf(stderr, "[DiscordRPC] sendFrame: write opcode failed\n"); return false; }
    if (!WriteFile(pipe, &len, 4, &written, NULL) || written != 4) { fprintf(stderr, "[DiscordRPC] sendFrame: write len failed\n"); return false; }
    if (len > 0 && (!WriteFile(pipe, data.data(), len, &written, NULL) || written != len)) { fprintf(stderr, "[DiscordRPC] sendFrame: write data failed\n"); return false; }
#else
    if (write(m_sock, &opcode, 4) != 4) { fprintf(stderr, "[DiscordRPC] sendFrame: write opcode failed\n"); return false; }
    if (write(m_sock, &len, 4) != 4) { fprintf(stderr, "[DiscordRPC] sendFrame: write len failed\n"); return false; }
    if (len > 0 && write(m_sock, data.data(), len) != (ssize_t)len) { fprintf(stderr, "[DiscordRPC] sendFrame: write data failed\n"); return false; }
#endif
    return true;
}

bool DiscordRPCManager::readFrame(uint32_t& opcode, std::string& data, int timeoutMs) {
    if (m_sock < 0) { fprintf(stderr, "[DiscordRPC] readFrame: not connected\n"); return false; }
#ifdef _WIN32
    HANDLE pipe = (HANDLE)(intptr_t)m_sock;
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) { fprintf(stderr, "[DiscordRPC] readFrame: PeekNamedPipe failed\n"); return false; }
    if (available < 8) {
        if (timeoutMs > 0) {
            Sleep(timeoutMs);
            if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) return false;
            if (available < 8) return false;
        } else {
            return false;
        }
    }
    DWORD read;
    if (!ReadFile(pipe, &opcode, 4, &read, NULL) || read != 4) { fprintf(stderr, "[DiscordRPC] readFrame: read opcode failed\n"); return false; }
    uint32_t len;
    if (!ReadFile(pipe, &len, 4, &read, NULL) || read != 4) { fprintf(stderr, "[DiscordRPC] readFrame: read len failed\n"); return false; }
    if (len > 0) {
        data.resize(len);
        if (!ReadFile(pipe, &data[0], len, &read, NULL) || read != len) { fprintf(stderr, "[DiscordRPC] readFrame: read data failed\n"); return false; }
    } else {
        data.clear();
    }
    fprintf(stderr, "[DiscordRPC] readFrame op=%u len=%u\n", opcode, len);
    return true;
#else
    struct pollfd pfd;
    pfd.fd = m_sock;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, timeoutMs);
    if (ret < 0) { fprintf(stderr, "[DiscordRPC] poll error: %s\n", strerror(errno)); return false; }
    if (ret == 0) return false;
    uint32_t len;
    ssize_t n = read(m_sock, &opcode, 4);
    if (n != 4) { if (n < 0) fprintf(stderr, "[DiscordRPC] read opcode error: %s\n", strerror(errno)); else fprintf(stderr, "[DiscordRPC] read opcode short: %zd\n", n); return false; }
    n = read(m_sock, &len, 4);
    if (n != 4) { if (n < 0) fprintf(stderr, "[DiscordRPC] read len error: %s\n", strerror(errno)); else fprintf(stderr, "[DiscordRPC] read len short: %zd\n", n); return false; }
    if (len > 0) {
        data.resize(len);
        n = read(m_sock, &data[0], len);
        if (n != (ssize_t)len) { if (n < 0) fprintf(stderr, "[DiscordRPC] read data error: %s\n", strerror(errno)); else fprintf(stderr, "[DiscordRPC] read data short: %zd/%u\n", n, len); return false; }
    } else {
        data.clear();
    }
    return true;
#endif
}

bool DiscordRPCManager::doHandshake() {
    json handshake;
    handshake["v"] = 1;
    handshake["client_id"] = m_clientId;
    std::string payload = handshake.dump();
    if (!sendFrame(0, payload)) { fprintf(stderr, "[DiscordRPC] Handshake send failed\n"); return false; }
    {
        uint32_t opcode;
        std::string response;
        if (!readFrame(opcode, response, 2000)) {
            fprintf(stderr, "[DiscordRPC] Handshake no response (timeout)\n");
            return false;
        }
        fprintf(stderr, "[DiscordRPC] Handshake got op=%u data=%.200s\n", opcode, response.c_str());
        if (opcode == 2) { fprintf(stderr, "[DiscordRPC] Handshake: server closed\n"); return false; }
        if (opcode == 3) { sendFrame(4, response); }
    }

    while (true) {
        uint32_t opcode;
        std::string response;
        if (!readFrame(opcode, response, 0)) break;
        fprintf(stderr, "[DiscordRPC] Handshake drain op=%u data=%.200s\n", opcode, response.c_str());
        if (opcode == 2) { fprintf(stderr, "[DiscordRPC] Handshake: server closed\n"); return false; }
        if (opcode == 3) { sendFrame(4, response); }
    }
    return true;
}

int DiscordRPCManager::reconnectDelayMs() const {
    int delay = 500 * (1 << m_reconnectAttempts);
    if (delay > MAX_RECONNECT_DELAY_MS) delay = MAX_RECONNECT_DELAY_MS;
    return delay;
}

void DiscordRPCManager::tryReconnect() {
    fprintf(stderr, "[DiscordRPC] Reconnect attempt %d\n", m_reconnectAttempts);
    if (connectToDiscord() && doHandshake()) {
        m_reconnectAttempts = 0;
        fprintf(stdout, "[DiscordRPC] Connected\n");
    } else {
        m_reconnectAttempts++;
        fprintf(stderr, "[DiscordRPC] Reconnect failed\n");
        disconnect();
    }
}

void DiscordRPCManager::init(const char* clientId) {
    if (m_initialized) { fprintf(stderr, "[DiscordRPC] init: already initialized\n"); return; }
    fprintf(stderr, "[DiscordRPC] init: clientId=%s\n", clientId);
    m_clientId = clientId;
    m_startTime = std::time(nullptr);
    tryReconnect();
    m_initialized = true;
    if (isConnected()) {
        update("Starting...", "neoDAW");
    } else {
        fprintf(stderr, "[DiscordRPC] init: not connected, will retry in update()\n");
    }
}

void DiscordRPCManager::update(const char* state, const char* details, const char* smallImageKey, const char* smallImageText) {
    if (!m_initialized) { fprintf(stderr, "[DiscordRPC] update: not initialized\n"); return; }
    fprintf(stderr, "[DiscordRPC] update: state=%s details=%s\n", state ? state : "null", details ? details : "null");
    if (!isConnected()) {
        fprintf(stderr, "[DiscordRPC] update: not connected, trying reconnect\n");
        tryReconnect();
        if (!isConnected()) { fprintf(stderr, "[DiscordRPC] update: reconnect failed\n"); return; }
    }

    uint32_t opcode;
    std::string incoming;
    while (readFrame(opcode, incoming, 0)) {
        if (opcode == 3) {
            sendFrame(4, incoming);
        } else if (opcode == 2) {
            fprintf(stderr, "[DiscordRPC] update: server closed connection\n");
            disconnect();
            return;
        } else if (opcode == 1) {
            try {
                json j = json::parse(incoming, nullptr, false);
                if (j.is_object() && j.contains("evt") && j["evt"].is_string() && j["evt"] == "ERROR") {
                    int code = j["data"].value("code", -1);
                    std::string msg = j["data"].value("message", "");
                    fprintf(stderr, "[DiscordRPC] ERROR: cmd=%s code=%d msg=%s\n",
                        j.value("cmd","").c_str(), code, msg.c_str());
                }
            } catch (json::exception& e) {
                fprintf(stderr, "[DiscordRPC] JSON parse error: %s\n", e.what());
            }
        }
    }

    if (!isConnected()) return;
    bool changed = false;
    if (m_lastState != state) { fprintf(stderr, "[DiscordRPC] state changed: '%s' -> '%s'\n", m_lastState.c_str(), state); m_lastState = state; changed = true; }
    if (m_lastDetails != details) { fprintf(stderr, "[DiscordRPC] details changed: '%s' -> '%s'\n", m_lastDetails.c_str(), details); m_lastDetails = details; changed = true; }
    std::string sk = smallImageKey ? smallImageKey : "";
    std::string st = smallImageText ? smallImageText : "";
    if (m_lastSmallKey != sk) { m_lastSmallKey = sk; changed = true; }
    if (m_lastSmallText != st) { m_lastSmallText = st; changed = true; }
    if (!changed) { return; }
    if (!isConnected()) return;
    json activity;
    activity["state"] = state;
    activity["details"] = details;
    activity["timestamps"]["start"] = m_startTime;
    activity["assets"]["large_image"] = "neodaw_logo";
    activity["assets"]["large_text"] = "neoDAW";
    if (smallImageKey) {
        activity["assets"]["small_image"] = smallImageKey;
        activity["assets"]["small_text"] = smallImageText;
    }
    activity["instance"] = true;
    json args;
    args["pid"] = (int64_t)getpid();
    args["activity"] = activity;
    json frame;
    frame["cmd"] = "SET_ACTIVITY";
    frame["args"] = args;
    frame["nonce"] = std::to_string(m_nonceCounter++);
    std::string payload = frame.dump();
    if (!sendFrame(1, payload)) {
        fprintf(stderr, "[DiscordRPC] update: sendFrame failed, disconnecting\n");
        disconnect();
    }
}

void DiscordRPCManager::shutdown() {
    if (!m_initialized) { fprintf(stderr, "[DiscordRPC] shutdown: not initialized\n"); return; }
    fprintf(stderr, "[DiscordRPC] shutdown\n");
    disconnect();
    m_initialized = false;
}
