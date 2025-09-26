#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <shared_mutex>
#include <chrono>
#include <set>

class IRCClient;
struct LogEntry;

// [SEQUENCE: 525] IRC Channel - manages channel state and log integration
class IRCChannel {
public:
    // [SEQUENCE: 526] Channel types
    enum class Type {
        NORMAL,         // Regular IRC channel (#channel)
        LOG_STREAM,     // Log streaming channel (#logs-*)
        PRIVATE         // Private message (not implemented in MVP1)
    };
    
    // [SEQUENCE: 527] Channel modes (simplified for MVP1)
    struct ChannelModes {
        bool moderated = false;      // +m - only voiced users can speak
        bool inviteOnly = false;     // +i - invite only
        bool topicProtected = true;  // +t - only ops can change topic
        int userLimit = 0;           // +l - user limit (0 = no limit)
    };
    
    // [SEQUENCE: 528] Constructor with channel name
    explicit IRCChannel(const std::string& name, Type type = Type::NORMAL);
    ~IRCChannel();
    
    // [SEQUENCE: 529] Client management
    void addClient(std::shared_ptr<IRCClient> client, bool isOperator = false);
    void removeClient(const std::string& nickname);
    bool hasClient(const std::string& nickname) const;
    std::shared_ptr<IRCClient> getClient(const std::string& nickname) const;
    
    // [SEQUENCE: 530] Message broadcasting
    void broadcast(const std::string& message, const std::string& sender = "");
    void broadcastExcept(const std::string& message, const std::string& exceptNick, const std::string& sender = "");
    void sendToClient(const std::string& nickname, const std::string& message);
    
    // [SEQUENCE: 531] Channel properties
    void setTopic(const std::string& topic, const std::string& setBy);
    const std::string& getTopic() const { return topic_; }
    const std::string& getTopicSetBy() const { return topicSetBy_; }
    std::chrono::system_clock::time_point getTopicSetTime() const { return topicSetTime_; }
    
    // [SEQUENCE: 532] Channel modes
    void setMode(char mode, bool enable, const std::string& param = "");
    std::string getModeString() const;
    const ChannelModes& getModes() const { return modes_; }
    
    // [SEQUENCE: 533] Operator management
    void addOperator(const std::string& nickname);
    void removeOperator(const std::string& nickname);
    bool isOperator(const std::string& nickname) const;
    
    // [SEQUENCE: 534] Log streaming integration
    void setLogFilter(const std::function<bool(const LogEntry&)>& filter);
    void enableLogStreaming(bool enable);
    bool isLogStreamingEnabled() const { return streamingEnabled_; }
    void processLogEntry(const LogEntry& entry);
    
    // [SEQUENCE: 535] Channel information
    const std::string& getName() const { return name_; }
    Type getType() const { return type_; }
    size_t getClientCount() const;
    std::vector<std::string> getClientList() const;
    std::string getCreationTime() const;
    
    // [SEQUENCE: 536] Log filter helpers for common patterns
    static std::function<bool(const LogEntry&)> createLevelFilter(const std::string& level);
    static std::function<bool(const LogEntry&)> createKeywordFilter(const std::string& keyword);
    static std::function<bool(const LogEntry&)> createRegexFilter(const std::string& pattern);
    
private:
    // [SEQUENCE: 537] Channel properties
    std::string name_;
    Type type_;
    std::string topic_;
    std::string topicSetBy_;
    std::chrono::system_clock::time_point topicSetTime_;
    std::chrono::system_clock::time_point creationTime_;
    
    // [SEQUENCE: 538] Channel modes
    ChannelModes modes_;
    
    // [SEQUENCE: 539] Client management
    std::map<std::string, std::shared_ptr<IRCClient>> clients_;
    std::set<std::string> operators_;
    
    // [SEQUENCE: 540] Log streaming
    bool streamingEnabled_;
    std::function<bool(const LogEntry&)> logFilter_;
    
    // [SEQUENCE: 541] Thread safety
    mutable std::shared_mutex mutex_;
    
    // [SEQUENCE: 542] Helper methods
    void sendNames(std::shared_ptr<IRCClient> client);
    std::string formatLogEntry(const LogEntry& entry) const;
};