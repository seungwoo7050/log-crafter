#include "IRCChannel.h"
#include "IRCClient.h"
#include "IRCCommandParser.h"
#include "LogBuffer.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>

// [SEQUENCE: 652] Constructor - initialize channel
IRCChannel::IRCChannel(const std::string& name, Type type)
    : name_(name)
    , type_(type)
    , creationTime_(std::chrono::system_clock::now())
    , streamingEnabled_(false) {
    
    // [SEQUENCE: 653] Set default topic for log channels
    if (type == Type::LOG_STREAM) {
        topic_ = "Log streaming channel - " + name;
        topicSetBy_ = "LogCrafter";
        topicSetTime_ = creationTime_;
    }
}

// [SEQUENCE: 654] Destructor
IRCChannel::~IRCChannel() = default;

// [SEQUENCE: 655] Add client to channel
void IRCChannel::addClient(std::shared_ptr<IRCClient> client, bool isOperator) {
    if (!client) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::string nickname = client->getNickname();
    clients_[nickname] = client;
    
    // [SEQUENCE: 656] First user becomes operator
    if (clients_.size() == 1 || isOperator) {
        operators_.insert(nickname);
    }
    
    lock.unlock();
    
    // [SEQUENCE: 657] Send channel join messages
    sendNames(client);
    
    // [SEQUENCE: 658] Send topic if set
    if (!topic_.empty()) {
        client->sendNumericReply(IRCCommandParser::RPL_TOPIC, 
                                name_ + " :" + topic_);
        
        // [SEQUENCE: 659] Send topic metadata
        std::ostringstream oss;
        oss << name_ << " " << topicSetBy_ << " " 
            << std::chrono::system_clock::to_time_t(topicSetTime_);
        client->sendNumericReply(IRCCommandParser::RPL_TOPICWHOTIME, oss.str());
    } else {
        client->sendNumericReply(IRCCommandParser::RPL_NOTOPIC, 
                                name_ + " :No topic is set");
    }
}

// [SEQUENCE: 660] Remove client from channel
void IRCChannel::removeClient(const std::string& nickname) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    clients_.erase(nickname);
    operators_.erase(nickname);
    
    // [SEQUENCE: 661] If channel is empty and not a system channel, it could be removed
    // (handled by channel manager)
}

// [SEQUENCE: 662] Check if client is in channel
bool IRCChannel::hasClient(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return clients_.find(nickname) != clients_.end();
}

// [SEQUENCE: 663] Get client by nickname
std::shared_ptr<IRCClient> IRCChannel::getClient(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clients_.find(nickname);
    if (it != clients_.end()) {
        return it->second;
    }
    return nullptr;
}

// [SEQUENCE: 664] Broadcast message to all clients
void IRCChannel::broadcast(const std::string& message, const std::string& sender) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [nick, client] : clients_) {
        if (!sender.empty() && nick == sender) {
            continue; // Don't echo back to sender
        }
        client->sendMessage(message);
    }
}

// [SEQUENCE: 665] Broadcast to all except specified client
void IRCChannel::broadcastExcept(const std::string& message, 
                                 const std::string& exceptNick, 
                                 const std::string& sender) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [nick, client] : clients_) {
        if (nick == exceptNick) {
            continue;
        }
        client->sendMessage(message);
    }
}

// [SEQUENCE: 666] Send message to specific client
void IRCChannel::sendToClient(const std::string& nickname, const std::string& message) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clients_.find(nickname);
    if (it != clients_.end()) {
        it->second->sendMessage(message);
    }
}

// [SEQUENCE: 667] Set channel topic
void IRCChannel::setTopic(const std::string& topic, const std::string& setBy) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    topic_ = topic;
    topicSetBy_ = setBy;
    topicSetTime_ = std::chrono::system_clock::now();
}

// [SEQUENCE: 668] Set channel mode
void IRCChannel::setMode(char mode, bool enable, const std::string& param) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    switch (mode) {
        case 'm': modes_.moderated = enable; break;
        case 'i': modes_.inviteOnly = enable; break;
        case 't': modes_.topicProtected = enable; break;
        case 'l': 
            if (enable && !param.empty()) {
                modes_.userLimit = std::stoi(param);
            } else {
                modes_.userLimit = 0;
            }
            break;
    }
}

// [SEQUENCE: 669] Get mode string
std::string IRCChannel::getModeString() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::string modeStr = "+";
    if (modes_.moderated) modeStr += "m";
    if (modes_.inviteOnly) modeStr += "i";
    if (modes_.topicProtected) modeStr += "t";
    if (modes_.userLimit > 0) modeStr += "l";
    
    if (modes_.userLimit > 0) {
        modeStr += " " + std::to_string(modes_.userLimit);
    }
    
    return modeStr;
}

// [SEQUENCE: 670] Add operator
void IRCChannel::addOperator(const std::string& nickname) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (clients_.find(nickname) != clients_.end()) {
        operators_.insert(nickname);
    }
}

// [SEQUENCE: 671] Remove operator
void IRCChannel::removeOperator(const std::string& nickname) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    operators_.erase(nickname);
}

// [SEQUENCE: 672] Check if user is operator
bool IRCChannel::isOperator(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return operators_.find(nickname) != operators_.end();
}

// [SEQUENCE: 673] Set log filter
void IRCChannel::setLogFilter(const std::function<bool(const LogEntry&)>& filter) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    logFilter_ = filter;
}

// [SEQUENCE: 674] Enable/disable log streaming
void IRCChannel::enableLogStreaming(bool enable) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    streamingEnabled_ = enable;
}

// [SEQUENCE: 675] Process log entry for streaming
void IRCChannel::processLogEntry(const LogEntry& entry) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (!streamingEnabled_ || clients_.empty()) {
        return;
    }
    
    // [SEQUENCE: 676] Apply filter if set
    if (logFilter_ && !logFilter_(entry)) {
        return;
    }
    
    // [SEQUENCE: 677] Format and broadcast log entry
    std::string formattedLog = formatLogEntry(entry);
    std::string message = IRCCommandParser::formatUserMessage(
        "LogBot", "log", "system", "PRIVMSG", name_, formattedLog
    );
    
    lock.unlock();
    broadcast(message);
}

// [SEQUENCE: 678] Get client count
size_t IRCChannel::getClientCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return clients_.size();
}

// [SEQUENCE: 679] Get client list
std::vector<std::string> IRCChannel::getClientList() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> nicknames;
    nicknames.reserve(clients_.size());
    
    for (const auto& [nick, client] : clients_) {
        std::string prefix = operators_.count(nick) ? "@" : "";
        nicknames.push_back(prefix + nick);
    }
    
    return nicknames;
}

// [SEQUENCE: 680] Get creation time string
std::string IRCChannel::getCreationTime() const {
    auto time_t = std::chrono::system_clock::to_time_t(creationTime_);
    return std::to_string(time_t);
}

// [SEQUENCE: 681] Create level filter
std::function<bool(const LogEntry&)> IRCChannel::createLevelFilter(const std::string& level) {
    return [level](const LogEntry& entry) {
        return entry.level == level;
    };
}

// [SEQUENCE: 682] Create keyword filter
std::function<bool(const LogEntry&)> IRCChannel::createKeywordFilter(const std::string& keyword) {
    return [keyword](const LogEntry& entry) {
        return entry.message.find(keyword) != std::string::npos;
    };
}

// [SEQUENCE: 683] Create regex filter
std::function<bool(const LogEntry&)> IRCChannel::createRegexFilter(const std::string& pattern) {
    std::regex regex(pattern);
    return [regex](const LogEntry& entry) {
        return std::regex_search(entry.message, regex);
    };
}

// [SEQUENCE: 684] Send channel names to client
void IRCChannel::sendNames(std::shared_ptr<IRCClient> client) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ostringstream namesReply;
    namesReply << "= " << name_ << " :";
    
    bool first = true;
    for (const auto& [nick, c] : clients_) {
        if (!first) namesReply << " ";
        if (operators_.count(nick)) namesReply << "@";
        namesReply << nick;
        first = false;
    }
    
    client->sendNumericReply(IRCCommandParser::RPL_NAMREPLY, namesReply.str());
    client->sendNumericReply(IRCCommandParser::RPL_ENDOFNAMES, 
                            name_ + " :End of /NAMES list");
}

// [SEQUENCE: 685] Format log entry for IRC display
std::string IRCChannel::formatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    
    // [SEQUENCE: 686] Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    struct tm* tm = localtime(&time_t);
    oss << "[" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "] ";
    
    // [SEQUENCE: 687] Add level if present
    if (!entry.level.empty()) {
        oss << entry.level << ": ";
    }
    
    // [SEQUENCE: 688] Add source if present
    if (!entry.source.empty()) {
        oss << "[" << entry.source << "] ";
    }
    
    // [SEQUENCE: 689] Add message
    oss << entry.message;
    
    return oss.str();
}