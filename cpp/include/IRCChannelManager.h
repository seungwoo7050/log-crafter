#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <functional>

class IRCChannel;
class IRCClient;
struct LogEntry;

// [SEQUENCE: 569] IRC Channel Manager - manages all channels in the server
class IRCChannelManager {
public:
    // [SEQUENCE: 570] Constructor
    IRCChannelManager();
    ~IRCChannelManager();
    
    // [SEQUENCE: 571] Channel lifecycle management
    std::shared_ptr<IRCChannel> createChannel(const std::string& name, IRCChannel::Type type = IRCChannel::Type::NORMAL);
    void removeChannel(const std::string& name);
    bool channelExists(const std::string& name) const;
    std::shared_ptr<IRCChannel> getChannel(const std::string& name) const;
    
    // [SEQUENCE: 572] Client channel operations
    bool joinChannel(std::shared_ptr<IRCClient> client, const std::string& channelName, const std::string& key = "");
    bool partChannel(std::shared_ptr<IRCClient> client, const std::string& channelName, const std::string& reason = "");
    void partAllChannels(std::shared_ptr<IRCClient> client, const std::string& reason = "");
    
    // [SEQUENCE: 573] Channel listing and discovery
    std::vector<std::string> getChannelList() const;
    std::vector<std::string> getClientChannels(const std::string& nickname) const;
    size_t getChannelCount() const;
    
    // [SEQUENCE: 574] Log streaming channel management
    void initializeLogChannels();
    void createLogStreamChannel(const std::string& name, const std::string& level);
    void createKeywordLogChannel(const std::string& name, const std::string& keyword);
    void updateLogChannelFilters();
    
    // [SEQUENCE: 575] Log entry distribution
    void distributeLogEntry(const LogEntry& entry);
    void registerLogCallback(std::function<void(const LogEntry&)> callback);
    
    // [SEQUENCE: 576] Channel search and filtering
    std::vector<std::shared_ptr<IRCChannel>> findChannelsByType(IRCChannel::Type type) const;
    std::vector<std::shared_ptr<IRCChannel>> findChannelsByPattern(const std::string& pattern) const;
    
    // [SEQUENCE: 577] Statistics
    struct ChannelStats {
        size_t totalChannels;
        size_t normalChannels;
        size_t logStreamChannels;
        size_t totalUsers;
        std::vector<std::pair<std::string, size_t>> topChannels; // name, user count
    };
    ChannelStats getStatistics() const;
    
private:
    // [SEQUENCE: 578] Channel storage
    std::unordered_map<std::string, std::shared_ptr<IRCChannel>> channels_;
    
    // [SEQUENCE: 579] Log distribution callback
    std::function<void(const LogEntry&)> logCallback_;
    
    // [SEQUENCE: 580] Thread safety
    mutable std::shared_mutex mutex_;
    
    // [SEQUENCE: 581] Helper methods
    bool isValidChannelName(const std::string& name) const;
    std::string normalizeChannelName(const std::string& name) const;
    void sendJoinMessages(std::shared_ptr<IRCChannel> channel, std::shared_ptr<IRCClient> client);
    void sendPartMessages(std::shared_ptr<IRCChannel> channel, std::shared_ptr<IRCClient> client, const std::string& reason);
    
    // [SEQUENCE: 582] Default log channels configuration
    struct LogChannelConfig {
        std::string name;
        std::string level;
        std::string description;
    };
    static const std::vector<LogChannelConfig> defaultLogChannels_;
};