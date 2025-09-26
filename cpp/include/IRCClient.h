#pragma once

#include <string>
#include <set>
#include <mutex>
#include <chrono>
#include <memory>

// [SEQUENCE: 510] IRC Client representation - manages individual client state
class IRCClient {
public:
    // [SEQUENCE: 511] Client states in IRC protocol
    enum class State {
        CONNECTED,      // Socket connected, no auth
        NICK_SET,       // NICK command received
        USER_SET,       // USER command received  
        AUTHENTICATED,  // Both NICK and USER set
        DISCONNECTED    // Client disconnected
    };
    
    // [SEQUENCE: 512] Constructor with socket info
    IRCClient(int fd, const std::string& address);
    ~IRCClient();
    
    // [SEQUENCE: 513] IRC protocol user information
    void setNickname(const std::string& nick);
    void setUsername(const std::string& user);
    void setRealname(const std::string& realname);
    void setHostname(const std::string& hostname);
    
    // [SEQUENCE: 514] Channel management
    void joinChannel(const std::string& channel);
    void partChannel(const std::string& channel);
    bool isInChannel(const std::string& channel) const;
    
    // [SEQUENCE: 515] Message handling
    void sendMessage(const std::string& message);
    void sendNumericReply(int code, const std::string& params);
    void sendErrorReply(int code, const std::string& params);
    
    // [SEQUENCE: 516] State management
    void setState(State state);
    State getState() const;
    bool isAuthenticated() const;
    void updateLastActivity();
    
    // [SEQUENCE: 517] Getters
    int getFd() const { return fd_; }
    const std::string& getAddress() const { return address_; }
    const std::string& getNickname() const { return nickname_; }
    const std::string& getUsername() const { return username_; }
    const std::string& getRealname() const { return realname_; }
    const std::string& getHostname() const { return hostname_; }
    const std::set<std::string>& getChannels() const { return channels_; }
    
    // [SEQUENCE: 518] Full client identifier for IRC protocol
    std::string getFullIdentifier() const;  // nick!user@host
    
    // [SEQUENCE: 519] Activity tracking
    std::chrono::steady_clock::time_point getLastActivity() const { return lastActivity_; }
    bool isIdle(std::chrono::seconds threshold) const;
    
private:
    // [SEQUENCE: 520] Client properties
    int fd_;
    std::string address_;
    std::string nickname_;
    std::string username_;
    std::string realname_;
    std::string hostname_;
    State state_;
    
    // [SEQUENCE: 521] Channel membership
    std::set<std::string> channels_;
    
    // [SEQUENCE: 522] Activity tracking
    std::chrono::steady_clock::time_point connectionTime_;
    std::chrono::steady_clock::time_point lastActivity_;
    
    // [SEQUENCE: 523] Thread safety
    mutable std::mutex mutex_;
    
    // [SEQUENCE: 524] Helper methods
    void sendRawMessage(const std::string& message);
};