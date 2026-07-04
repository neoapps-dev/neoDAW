#pragma once
#include <cstdint>
#include <string>
class DiscordRPCManager {
public:
    DiscordRPCManager();
    ~DiscordRPCManager();
    void init(const char* clientId);
    void update(const char* state, const char* details, const char* smallImageKey = nullptr, const char* smallImageText = nullptr);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

private:
    bool connectToDiscord();
    void disconnect();
    bool doHandshake();
    bool sendFrame(uint32_t opcode, const std::string& data);
    bool readFrame(uint32_t& opcode, std::string& data, int timeoutMs = 0);
    bool isConnected() const { return m_sock >= 0; }
    void tryReconnect();
    int reconnectDelayMs() const;
    static constexpr int MAX_RECONNECT_DELAY_MS = 16000;
    bool m_initialized = false;
    int m_sock = -1;
    int64_t m_startTime = 0;
    int64_t m_nonceCounter = 1;
    std::string m_clientId;
    std::string m_lastState;
    std::string m_lastDetails;
    std::string m_lastSmallKey;
    std::string m_lastSmallText;
    int m_reconnectAttempts = 0;
};
