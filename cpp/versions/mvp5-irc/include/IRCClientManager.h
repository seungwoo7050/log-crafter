#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <chrono>
#include <set>

class IRCClient;

// [SEQUENCE: 583] IRC Client Manager - manages all connected clients
class IRCClientManager {
public:
    // [SEQUENCE: 584] Constructor
    IRCClientManager();
    ~IRCClientManager();
    
    // [SEQUENCE: 585] Client lifecycle management
    std::shared_ptr<IRCClient> addClient(int fd, const std::string& address);
    void removeClient(int fd);
    void removeClient(const std::string& nickname);
    bool clientExists(int fd) const;
    bool nicknameExists(const std::string& nickname) const;
    
    // [SEQUENCE: 586] Client retrieval
    std::shared_ptr<IRCClient> getClientByFd(int fd) const;
    std::shared_ptr<IRCClient> getClientByNickname(const std::string& nickname) const;
    std::vector<std::shared_ptr<IRCClient>> getAllClients() const;
    
    // [SEQUENCE: 587] Nickname management
    bool isNicknameAvailable(const std::string& nickname) const;
    bool changeNickname(const std::string& oldNick, const std::string& newNick);
    void registerNickname(int fd, const std::string& nickname);
    
    // [SEQUENCE: 588] Client search and filtering
    std::vector<std::shared_ptr<IRCClient>> findClientsByPattern(const std::string& pattern) const;
    std::vector<std::shared_ptr<IRCClient>> findClientsByHost(const std::string& host) const;
    std::vector<std::shared_ptr<IRCClient>> getAuthenticatedClients() const;
    
    // [SEQUENCE: 589] Broadcasting
    void broadcastToAll(const std::string& message, const std::string& excludeNick = "");
    void broadcastToAuthenticated(const std::string& message, const std::string& excludeNick = "");
    
    // [SEQUENCE: 590] Client activity tracking
    void updateClientActivity(int fd);
    std::vector<std::shared_ptr<IRCClient>> getIdleClients(std::chrono::seconds threshold) const;
    void disconnectIdleClients(std::chrono::seconds threshold);
    
    // [SEQUENCE: 591] Statistics
    struct ClientStats {
        size_t totalConnected;
        size_t authenticated;
        size_t unauthenticated;
        size_t uniqueHosts;
        std::chrono::seconds averageConnectionTime;
        std::vector<std::pair<std::string, size_t>> topHosts; // host, count
    };
    ClientStats getStatistics() const;
    
    // [SEQUENCE: 592] Connection limits
    bool canAcceptConnection(const std::string& address) const;
    void setMaxConnectionsPerHost(size_t limit);
    void setGlobalMaxConnections(size_t limit);
    
private:
    // [SEQUENCE: 593] Client storage - dual indexing for fast lookup
    std::unordered_map<int, std::shared_ptr<IRCClient>> clientsByFd_;
    std::unordered_map<std::string, std::shared_ptr<IRCClient>> clientsByNick_;
    
    // [SEQUENCE: 594] Connection tracking
    std::unordered_map<std::string, std::set<int>> connectionsByHost_;
    
    // [SEQUENCE: 595] Connection limits
    size_t maxConnectionsPerHost_ = 10;
    size_t globalMaxConnections_ = 1000;
    
    // [SEQUENCE: 596] Thread safety
    mutable std::shared_mutex mutex_;
    
    // [SEQUENCE: 597] Helper methods
    void cleanupClient(std::shared_ptr<IRCClient> client);
    std::string normalizeNickname(const std::string& nick) const;
    bool isValidNickname(const std::string& nick) const;
    void updateHostTracking(const std::string& host, int fd, bool add);
};