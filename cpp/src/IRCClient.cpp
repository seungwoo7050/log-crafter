#include "IRCClient.h"
#include "IRCCommandParser.h"
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

// [SEQUENCE: 626] Constructor - initialize client with socket info
IRCClient::IRCClient(int fd, const std::string& address)
    : fd_(fd)
    , address_(address)
    , state_(State::CONNECTED)
    , connectionTime_(std::chrono::steady_clock::now())
    , lastActivity_(std::chrono::steady_clock::now()) {
    
    // [SEQUENCE: 627] Extract hostname from address (IP:port)
    size_t colonPos = address.find(':');
    if (colonPos != std::string::npos) {
        hostname_ = address.substr(0, colonPos);
    } else {
        hostname_ = address;
    }
}

// [SEQUENCE: 628] Destructor - ensure socket is closed
IRCClient::~IRCClient() {
    if (fd_ != -1) {
        close(fd_);
    }
}

// [SEQUENCE: 629] Set nickname with validation
void IRCClient::setNickname(const std::string& nick) {
    std::lock_guard<std::mutex> lock(mutex_);
    nickname_ = nick;
    
    // [SEQUENCE: 630] Update state if progressing through handshake
    if (state_ == State::CONNECTED) {
        state_ = State::NICK_SET;
    } else if (state_ == State::USER_SET) {
        state_ = State::AUTHENTICATED;
    }
    
    updateLastActivity();
}

// [SEQUENCE: 631] Set username
void IRCClient::setUsername(const std::string& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    username_ = user;
    
    // [SEQUENCE: 632] Update state if progressing through handshake
    if (state_ == State::CONNECTED) {
        state_ = State::USER_SET;
    } else if (state_ == State::NICK_SET) {
        state_ = State::AUTHENTICATED;
    }
    
    updateLastActivity();
}

// [SEQUENCE: 633] Set realname
void IRCClient::setRealname(const std::string& realname) {
    std::lock_guard<std::mutex> lock(mutex_);
    realname_ = realname;
    updateLastActivity();
}

// [SEQUENCE: 634] Set hostname
void IRCClient::setHostname(const std::string& hostname) {
    std::lock_guard<std::mutex> lock(mutex_);
    hostname_ = hostname;
    updateLastActivity();
}

// [SEQUENCE: 635] Join a channel
void IRCClient::joinChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.insert(channel);
    updateLastActivity();
}

// [SEQUENCE: 636] Leave a channel
void IRCClient::partChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.erase(channel);
    updateLastActivity();
}

// [SEQUENCE: 637] Check channel membership
bool IRCClient::isInChannel(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_.find(channel) != channels_.end();
}

// [SEQUENCE: 638] Send message to client
void IRCClient::sendMessage(const std::string& message) {
    if (fd_ == -1) {
        return;
    }
    
    // [SEQUENCE: 639] Ensure message ends with CRLF
    std::string fullMessage = message;
    if (fullMessage.size() < 2 || 
        fullMessage.substr(fullMessage.size() - 2) != "\r\n") {
        fullMessage += "\r\n";
    }
    
    sendRawMessage(fullMessage);
}

// [SEQUENCE: 640] Send numeric reply
void IRCClient::sendNumericReply(int code, const std::string& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string serverName = "logcrafter-irc";
    std::string nick = nickname_.empty() ? "*" : nickname_;
    
    std::string reply = IRCCommandParser::formatReply(serverName, nick, code, params);
    sendMessage(reply);
}

// [SEQUENCE: 641] Send error reply
void IRCClient::sendErrorReply(int code, const std::string& params) {
    sendNumericReply(code, params);
}

// [SEQUENCE: 642] Update client state
void IRCClient::setState(State state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
    updateLastActivity();
}

// [SEQUENCE: 643] Get current state
IRCClient::State IRCClient::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

// [SEQUENCE: 644] Check if authenticated
bool IRCClient::isAuthenticated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == State::AUTHENTICATED;
}

// [SEQUENCE: 645] Update last activity time
void IRCClient::updateLastActivity() {
    lastActivity_ = std::chrono::steady_clock::now();
}

// [SEQUENCE: 646] Get full client identifier (nick!user@host)
std::string IRCClient::getFullIdentifier() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (nickname_.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << nickname_ << "!" << username_ << "@" << hostname_;
    return oss.str();
}

// [SEQUENCE: 647] Check if client is idle
bool IRCClient::isIdle(std::chrono::seconds threshold) const {
    auto now = std::chrono::steady_clock::now();
    auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity_);
    return idleTime > threshold;
}

// [SEQUENCE: 648] Send raw message to socket
void IRCClient::sendRawMessage(const std::string& message) {
    if (fd_ == -1) {
        return;
    }
    
    // [SEQUENCE: 649] Send with error handling
    ssize_t totalSent = 0;
    ssize_t messageLen = message.length();
    
    while (totalSent < messageLen) {
        ssize_t sent = send(fd_, message.c_str() + totalSent, 
                           messageLen - totalSent, MSG_NOSIGNAL);
        
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // [SEQUENCE: 650] Non-blocking socket, try again
                continue;
            }
            // [SEQUENCE: 651] Error or connection closed
            std::cerr << "Error sending to client " << nickname_ 
                     << ": " << strerror(errno) << std::endl;
            break;
        }
        
        totalSent += sent;
    }
}