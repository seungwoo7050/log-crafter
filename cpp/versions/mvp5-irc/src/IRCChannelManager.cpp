#include "IRCChannelManager.h"
#include "IRCChannel.h"
#include "IRCClient.h"
#include "LogBuffer.h"
#include <algorithm>
#include <iostream>

// [SEQUENCE: 701] Default log channel configurations
const std::vector<IRCChannelManager::LogChannelConfig> IRCChannelManager::defaultLogChannels_ = {
    {"#logs-all", "*", "All log messages"},
    {"#logs-error", "ERROR", "Error level logs only"},
    {"#logs-warning", "WARNING", "Warning level logs only"},
    {"#logs-info", "INFO", "Info level logs only"},
    {"#logs-debug", "DEBUG", "Debug level logs only"}
};

// [SEQUENCE: 702] Constructor
IRCChannelManager::IRCChannelManager() {
    // Channels will be initialized when IRC server starts
}

// [SEQUENCE: 703] Destructor
IRCChannelManager::~IRCChannelManager() = default;

// [SEQUENCE: 704] Create a new channel
std::shared_ptr<IRCChannel> IRCChannelManager::createChannel(const std::string& name, 
                                                             IRCChannel::Type type) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::string normalizedName = normalizeChannelName(name);
    
    // [SEQUENCE: 705] Check if channel already exists
    if (channels_.find(normalizedName) != channels_.end()) {
        return channels_[normalizedName];
    }
    
    // [SEQUENCE: 706] Create new channel
    auto channel = std::make_shared<IRCChannel>(normalizedName, type);
    channels_[normalizedName] = channel;
    
    return channel;
}

// [SEQUENCE: 707] Remove a channel
void IRCChannelManager::removeChannel(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::string normalizedName = normalizeChannelName(name);
    
    // [SEQUENCE: 708] Don't remove system log channels
    if (normalizedName.find("#logs-") == 0) {
        return;
    }
    
    channels_.erase(normalizedName);
}

// [SEQUENCE: 709] Check if channel exists
bool IRCChannelManager::channelExists(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::string normalizedName = normalizeChannelName(name);
    return channels_.find(normalizedName) != channels_.end();
}

// [SEQUENCE: 710] Get channel by name
std::shared_ptr<IRCChannel> IRCChannelManager::getChannel(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::string normalizedName = normalizeChannelName(name);
    auto it = channels_.find(normalizedName);
    
    if (it != channels_.end()) {
        return it->second;
    }
    
    return nullptr;
}

// [SEQUENCE: 711] Join client to channel
bool IRCChannelManager::joinChannel(std::shared_ptr<IRCClient> client, 
                                   const std::string& channelName, 
                                   const std::string& key) {
    if (!client || !client->isAuthenticated()) {
        return false;
    }
    
    // [SEQUENCE: 712] Create channel if it doesn't exist
    std::shared_ptr<IRCChannel> channel;
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        std::string normalizedName = normalizeChannelName(channelName);
        auto it = channels_.find(normalizedName);
        
        if (it == channels_.end()) {
            // [SEQUENCE: 713] Auto-create normal channels
            if (normalizedName.find("#logs-") != 0) {
                channel = std::make_shared<IRCChannel>(normalizedName, IRCChannel::Type::NORMAL);
                channels_[normalizedName] = channel;
            } else {
                return false; // Can't create log channels manually
            }
        } else {
            channel = it->second;
        }
    }
    
    // [SEQUENCE: 714] Add client to channel
    channel->addClient(client);
    client->joinChannel(channelName);
    
    // [SEQUENCE: 715] Send join notifications
    sendJoinMessages(channel, client);
    
    return true;
}

// [SEQUENCE: 716] Part client from channel
bool IRCChannelManager::partChannel(std::shared_ptr<IRCClient> client, 
                                   const std::string& channelName, 
                                   const std::string& reason) {
    if (!client) {
        return false;
    }
    
    auto channel = getChannel(channelName);
    if (!channel || !channel->hasClient(client->getNickname())) {
        return false;
    }
    
    // [SEQUENCE: 717] Send part notifications
    sendPartMessages(channel, client, reason);
    
    // [SEQUENCE: 718] Remove client from channel
    channel->removeClient(client->getNickname());
    client->partChannel(channelName);
    
    // [SEQUENCE: 719] Remove empty non-system channels
    if (channel->getClientCount() == 0 && channelName.find("#logs-") != 0) {
        removeChannel(channelName);
    }
    
    return true;
}

// [SEQUENCE: 720] Part client from all channels
void IRCChannelManager::partAllChannels(std::shared_ptr<IRCClient> client, 
                                       const std::string& reason) {
    if (!client) {
        return;
    }
    
    auto clientChannels = client->getChannels();
    for (const auto& channelName : clientChannels) {
        partChannel(client, channelName, reason);
    }
}

// [SEQUENCE: 721] Get list of all channels
std::vector<std::string> IRCChannelManager::getChannelList() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> channelNames;
    channelNames.reserve(channels_.size());
    
    for (const auto& [name, channel] : channels_) {
        channelNames.push_back(name);
    }
    
    return channelNames;
}

// [SEQUENCE: 722] Get channels for a specific client
std::vector<std::string> IRCChannelManager::getClientChannels(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> clientChannels;
    
    for (const auto& [name, channel] : channels_) {
        if (channel->hasClient(nickname)) {
            clientChannels.push_back(name);
        }
    }
    
    return clientChannels;
}

// [SEQUENCE: 723] Get channel count
size_t IRCChannelManager::getChannelCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return channels_.size();
}

// [SEQUENCE: 724] Initialize default log channels
void IRCChannelManager::initializeLogChannels() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& config : defaultLogChannels_) {
        auto channel = std::make_shared<IRCChannel>(config.name, IRCChannel::Type::LOG_STREAM);
        channel->setTopic(config.description, "LogCrafter");
        channel->enableLogStreaming(true);
        
        // [SEQUENCE: 725] Set up filter based on level
        if (config.level != "*") {
            channel->setLogFilter(IRCChannel::createLevelFilter(config.level));
        }
        
        channels_[config.name] = channel;
    }
}

// [SEQUENCE: 726] Create log stream channel with level filter
void IRCChannelManager::createLogStreamChannel(const std::string& name, const std::string& level) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto channel = std::make_shared<IRCChannel>(name, IRCChannel::Type::LOG_STREAM);
    channel->setTopic("Log stream for level: " + level, "LogCrafter");
    channel->enableLogStreaming(true);
    channel->setLogFilter(IRCChannel::createLevelFilter(level));
    
    channels_[name] = channel;
}

// [SEQUENCE: 727] Create keyword log channel
void IRCChannelManager::createKeywordLogChannel(const std::string& name, const std::string& keyword) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto channel = std::make_shared<IRCChannel>(name, IRCChannel::Type::LOG_STREAM);
    channel->setTopic("Log stream for keyword: " + keyword, "LogCrafter");
    channel->enableLogStreaming(true);
    channel->setLogFilter(IRCChannel::createKeywordFilter(keyword));
    
    channels_[name] = channel;
}

// [SEQUENCE: 728] Update log channel filters (placeholder for dynamic updates)
void IRCChannelManager::updateLogChannelFilters() {
    // This method can be used to dynamically update filters based on configuration
}

// [SEQUENCE: 729] Distribute log entry to all relevant channels
void IRCChannelManager::distributeLogEntry(const LogEntry& entry) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [name, channel] : channels_) {
        if (channel->getType() == IRCChannel::Type::LOG_STREAM && 
            channel->isLogStreamingEnabled()) {
            channel->processLogEntry(entry);
        }
    }
}

// [SEQUENCE: 730] Register log callback
void IRCChannelManager::registerLogCallback(std::function<void(const LogEntry&)> callback) {
    logCallback_ = callback;
}

// [SEQUENCE: 731] Find channels by type
std::vector<std::shared_ptr<IRCChannel>> IRCChannelManager::findChannelsByType(IRCChannel::Type type) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCChannel>> result;
    
    for (const auto& [name, channel] : channels_) {
        if (channel->getType() == type) {
            result.push_back(channel);
        }
    }
    
    return result;
}

// [SEQUENCE: 732] Find channels by pattern
std::vector<std::shared_ptr<IRCChannel>> IRCChannelManager::findChannelsByPattern(const std::string& pattern) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCChannel>> result;
    
    for (const auto& [name, channel] : channels_) {
        if (name.find(pattern) != std::string::npos) {
            result.push_back(channel);
        }
    }
    
    return result;
}

// [SEQUENCE: 733] Get channel statistics
IRCChannelManager::ChannelStats IRCChannelManager::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    ChannelStats stats;
    stats.totalChannels = channels_.size();
    stats.totalUsers = 0;
    
    std::vector<std::pair<std::string, size_t>> channelSizes;
    
    for (const auto& [name, channel] : channels_) {
        size_t clientCount = channel->getClientCount();
        stats.totalUsers += clientCount;
        
        if (channel->getType() == IRCChannel::Type::NORMAL) {
            stats.normalChannels++;
        } else if (channel->getType() == IRCChannel::Type::LOG_STREAM) {
            stats.logStreamChannels++;
        }
        
        channelSizes.push_back({name, clientCount});
    }
    
    // [SEQUENCE: 734] Sort by user count and get top channels
    std::sort(channelSizes.begin(), channelSizes.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    size_t topCount = std::min(size_t(10), channelSizes.size());
    stats.topChannels.assign(channelSizes.begin(), channelSizes.begin() + topCount);
    
    return stats;
}

// [SEQUENCE: 735] Check if channel name is valid
bool IRCChannelManager::isValidChannelName(const std::string& name) const {
    if (name.empty() || name.length() > 50) {
        return false;
    }
    
    if (name[0] != '#' && name[0] != '&') {
        return false;
    }
    
    for (char c : name) {
        if (c == ' ' || c == ',' || c == '\x07' || c < 32) {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 736] Normalize channel name
std::string IRCChannelManager::normalizeChannelName(const std::string& name) const {
    // For now, just return as-is. Could add case normalization later.
    return name;
}

// [SEQUENCE: 737] Send join messages
void IRCChannelManager::sendJoinMessages(std::shared_ptr<IRCChannel> channel, 
                                         std::shared_ptr<IRCClient> client) {
    // JOIN notification to all users
    std::string joinMsg = ":" + client->getFullIdentifier() + " JOIN :" + channel->getName();
    channel->broadcast(joinMsg);
}

// [SEQUENCE: 738] Send part messages
void IRCChannelManager::sendPartMessages(std::shared_ptr<IRCChannel> channel, 
                                        std::shared_ptr<IRCClient> client, 
                                        const std::string& reason) {
    // PART notification to all users
    std::string partMsg = ":" + client->getFullIdentifier() + " PART " + channel->getName();
    if (!reason.empty()) {
        partMsg += " :" + reason;
    }
    channel->broadcast(partMsg);
}