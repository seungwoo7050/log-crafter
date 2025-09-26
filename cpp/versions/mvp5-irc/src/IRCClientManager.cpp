#include "IRCClientManager.h"
#include "IRCClient.h"
#include <algorithm>
#include <regex>
#include <iostream>

// [SEQUENCE: 739] Constructor
IRCClientManager::IRCClientManager() 
    : maxConnectionsPerHost_(10)
    , globalMaxConnections_(1000) {
}

// [SEQUENCE: 740] Destructor
IRCClientManager::~IRCClientManager() {
    // Clients will be cleaned up automatically
}

// [SEQUENCE: 741] Add new client
std::shared_ptr<IRCClient> IRCClientManager::addClient(int fd, const std::string& address) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // [SEQUENCE: 742] Check global connection limit
    if (clientsByFd_.size() >= globalMaxConnections_) {
        return nullptr;
    }
    
    // [SEQUENCE: 743] Extract host from address
    std::string host = address;
    size_t colonPos = address.find(':');
    if (colonPos != std::string::npos) {
        host = address.substr(0, colonPos);
    }
    
    // [SEQUENCE: 744] Check per-host connection limit
    if (connectionsByHost_[host].size() >= maxConnectionsPerHost_) {
        return nullptr;
    }
    
    // [SEQUENCE: 745] Create new client
    auto client = std::make_shared<IRCClient>(fd, address);
    clientsByFd_[fd] = client;
    connectionsByHost_[host].insert(fd);
    
    return client;
}

// [SEQUENCE: 746] Remove client by file descriptor
void IRCClientManager::removeClient(int fd) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByFd_.find(fd);
    if (it == clientsByFd_.end()) {
        return;
    }
    
    auto client = it->second;
    cleanupClient(client);
    clientsByFd_.erase(it);
}

// [SEQUENCE: 747] Remove client by nickname
void IRCClientManager::removeClient(const std::string& nickname) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByNick_.find(normalizeNickname(nickname));
    if (it == clientsByNick_.end()) {
        return;
    }
    
    auto client = it->second;
    cleanupClient(client);
    clientsByFd_.erase(client->getFd());
}

// [SEQUENCE: 748] Check if client exists by fd
bool IRCClientManager::clientExists(int fd) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return clientsByFd_.find(fd) != clientsByFd_.end();
}

// [SEQUENCE: 749] Check if nickname exists
bool IRCClientManager::nicknameExists(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return clientsByNick_.find(normalizeNickname(nickname)) != clientsByNick_.end();
}

// [SEQUENCE: 750] Get client by file descriptor
std::shared_ptr<IRCClient> IRCClientManager::getClientByFd(int fd) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByFd_.find(fd);
    if (it != clientsByFd_.end()) {
        return it->second;
    }
    return nullptr;
}

// [SEQUENCE: 751] Get client by nickname
std::shared_ptr<IRCClient> IRCClientManager::getClientByNickname(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByNick_.find(normalizeNickname(nickname));
    if (it != clientsByNick_.end()) {
        return it->second;
    }
    return nullptr;
}

// [SEQUENCE: 752] Get all clients
std::vector<std::shared_ptr<IRCClient>> IRCClientManager::getAllClients() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCClient>> clients;
    clients.reserve(clientsByFd_.size());
    
    for (const auto& [fd, client] : clientsByFd_) {
        clients.push_back(client);
    }
    
    return clients;
}

// [SEQUENCE: 753] Check nickname availability
bool IRCClientManager::isNicknameAvailable(const std::string& nickname) const {
    return !nicknameExists(nickname) && isValidNickname(nickname);
}

// [SEQUENCE: 754] Change client nickname
bool IRCClientManager::changeNickname(const std::string& oldNick, const std::string& newNick) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::string normalizedOld = normalizeNickname(oldNick);
    std::string normalizedNew = normalizeNickname(newNick);
    
    // [SEQUENCE: 755] Check if new nickname is available
    if (clientsByNick_.find(normalizedNew) != clientsByNick_.end()) {
        return false;
    }
    
    // [SEQUENCE: 756] Find client with old nickname
    auto it = clientsByNick_.find(normalizedOld);
    if (it == clientsByNick_.end()) {
        return false;
    }
    
    // [SEQUENCE: 757] Update nickname mapping
    auto client = it->second;
    clientsByNick_.erase(it);
    clientsByNick_[normalizedNew] = client;
    
    return true;
}

// [SEQUENCE: 758] Register nickname for client
void IRCClientManager::registerNickname(int fd, const std::string& nickname) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByFd_.find(fd);
    if (it == clientsByFd_.end()) {
        return;
    }
    
    std::string normalized = normalizeNickname(nickname);
    clientsByNick_[normalized] = it->second;
}

// [SEQUENCE: 759] Find clients by pattern
std::vector<std::shared_ptr<IRCClient>> IRCClientManager::findClientsByPattern(const std::string& pattern) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCClient>> results;
    
    // [SEQUENCE: 760] Convert pattern to regex (simple wildcard support)
    std::string regexPattern = pattern;
    std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
    regexPattern = ".*" + regexPattern + ".*";
    
    try {
        std::regex re(regexPattern, std::regex_constants::icase);
        
        for (const auto& [nick, client] : clientsByNick_) {
            if (std::regex_match(nick, re)) {
                results.push_back(client);
            }
        }
    } catch (const std::regex_error& e) {
        // Invalid pattern, return empty results
    }
    
    return results;
}

// [SEQUENCE: 761] Find clients by host
std::vector<std::shared_ptr<IRCClient>> IRCClientManager::findClientsByHost(const std::string& host) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCClient>> results;
    
    auto it = connectionsByHost_.find(host);
    if (it != connectionsByHost_.end()) {
        for (int fd : it->second) {
            auto clientIt = clientsByFd_.find(fd);
            if (clientIt != clientsByFd_.end()) {
                results.push_back(clientIt->second);
            }
        }
    }
    
    return results;
}

// [SEQUENCE: 762] Get authenticated clients
std::vector<std::shared_ptr<IRCClient>> IRCClientManager::getAuthenticatedClients() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCClient>> results;
    
    for (const auto& [fd, client] : clientsByFd_) {
        if (client->isAuthenticated()) {
            results.push_back(client);
        }
    }
    
    return results;
}

// [SEQUENCE: 763] Broadcast to all clients
void IRCClientManager::broadcastToAll(const std::string& message, const std::string& excludeNick) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [fd, client] : clientsByFd_) {
        if (!excludeNick.empty() && client->getNickname() == excludeNick) {
            continue;
        }
        client->sendMessage(message);
    }
}

// [SEQUENCE: 764] Broadcast to authenticated clients
void IRCClientManager::broadcastToAuthenticated(const std::string& message, const std::string& excludeNick) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [fd, client] : clientsByFd_) {
        if (!client->isAuthenticated()) {
            continue;
        }
        if (!excludeNick.empty() && client->getNickname() == excludeNick) {
            continue;
        }
        client->sendMessage(message);
    }
}

// [SEQUENCE: 765] Update client activity
void IRCClientManager::updateClientActivity(int fd) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = clientsByFd_.find(fd);
    if (it != clientsByFd_.end()) {
        it->second->updateLastActivity();
    }
}

// [SEQUENCE: 766] Get idle clients
std::vector<std::shared_ptr<IRCClient>> IRCClientManager::getIdleClients(std::chrono::seconds threshold) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IRCClient>> idleClients;
    
    for (const auto& [fd, client] : clientsByFd_) {
        if (client->isIdle(threshold)) {
            idleClients.push_back(client);
        }
    }
    
    return idleClients;
}

// [SEQUENCE: 767] Disconnect idle clients
void IRCClientManager::disconnectIdleClients(std::chrono::seconds threshold) {
    auto idleClients = getIdleClients(threshold);
    
    for (const auto& client : idleClients) {
        removeClient(client->getFd());
    }
}

// [SEQUENCE: 768] Get client statistics
IRCClientManager::ClientStats IRCClientManager::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    ClientStats stats;
    stats.totalConnected = clientsByFd_.size();
    stats.authenticated = 0;
    stats.unauthenticated = 0;
    
    std::map<std::string, size_t> hostCounts;
    std::chrono::seconds totalConnectionTime(0);
    
    for (const auto& [fd, client] : clientsByFd_) {
        if (client->isAuthenticated()) {
            stats.authenticated++;
        } else {
            stats.unauthenticated++;
        }
        
        // Extract host
        std::string host = client->getAddress();
        size_t colonPos = host.find(':');
        if (colonPos != std::string::npos) {
            host = host.substr(0, colonPos);
        }
        hostCounts[host]++;
    }
    
    stats.uniqueHosts = hostCounts.size();
    
    // [SEQUENCE: 769] Calculate average connection time
    if (!clientsByFd_.empty()) {
        // Note: This is simplified - would need connection time tracking in IRCClient
        stats.averageConnectionTime = std::chrono::seconds(300); // Placeholder
    }
    
    // [SEQUENCE: 770] Get top hosts
    std::vector<std::pair<std::string, size_t>> sortedHosts(hostCounts.begin(), hostCounts.end());
    std::sort(sortedHosts.begin(), sortedHosts.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    size_t topCount = std::min(size_t(5), sortedHosts.size());
    stats.topHosts.assign(sortedHosts.begin(), sortedHosts.begin() + topCount);
    
    return stats;
}

// [SEQUENCE: 771] Check if can accept connection
bool IRCClientManager::canAcceptConnection(const std::string& address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (clientsByFd_.size() >= globalMaxConnections_) {
        return false;
    }
    
    std::string host = address;
    size_t colonPos = address.find(':');
    if (colonPos != std::string::npos) {
        host = address.substr(0, colonPos);
    }
    
    auto it = connectionsByHost_.find(host);
    if (it != connectionsByHost_.end() && it->second.size() >= maxConnectionsPerHost_) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: 772] Set max connections per host
void IRCClientManager::setMaxConnectionsPerHost(size_t limit) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    maxConnectionsPerHost_ = limit;
}

// [SEQUENCE: 773] Set global max connections
void IRCClientManager::setGlobalMaxConnections(size_t limit) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    globalMaxConnections_ = limit;
}

// [SEQUENCE: 774] Clean up client data
void IRCClientManager::cleanupClient(std::shared_ptr<IRCClient> client) {
    if (!client) return;
    
    // [SEQUENCE: 775] Remove from nickname map
    std::string nick = client->getNickname();
    if (!nick.empty()) {
        clientsByNick_.erase(normalizeNickname(nick));
    }
    
    // [SEQUENCE: 776] Remove from host tracking
    std::string host = client->getAddress();
    size_t colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        host = host.substr(0, colonPos);
    }
    updateHostTracking(host, client->getFd(), false);
}

// [SEQUENCE: 777] Normalize nickname
std::string IRCClientManager::normalizeNickname(const std::string& nick) const {
    // IRC nicknames are case-insensitive
    std::string normalized = nick;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

// [SEQUENCE: 778] Validate nickname
bool IRCClientManager::isValidNickname(const std::string& nick) const {
    if (nick.empty() || nick.length() > 9) {
        return false;
    }
    
    char first = nick[0];
    if (!std::isalpha(first) && first != '_' && first != '[' && 
        first != ']' && first != '{' && first != '}' && 
        first != '\\' && first != '|' && first != '^') {
        return false;
    }
    
    for (size_t i = 1; i < nick.length(); ++i) {
        char c = nick[i];
        if (!std::isalnum(c) && c != '_' && c != '-' && 
            c != '[' && c != ']' && c != '{' && c != '}' && 
            c != '\\' && c != '|' && c != '^') {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 779] Update host tracking
void IRCClientManager::updateHostTracking(const std::string& host, int fd, bool add) {
    if (add) {
        connectionsByHost_[host].insert(fd);
    } else {
        auto it = connectionsByHost_.find(host);
        if (it != connectionsByHost_.end()) {
            it->second.erase(fd);
            if (it->second.empty()) {
                connectionsByHost_.erase(it);
            }
        }
    }
}