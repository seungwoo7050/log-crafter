#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>

class LogBuffer;
class ThreadPool;
class IRCChannelManager;
class IRCClientManager;
class IRCCommandParser;
class IRCCommandHandler;

// [SEQUENCE: 500] IRC Server main class - manages IRC protocol integration
class IRCServer {
public:
    static constexpr int DEFAULT_IRC_PORT = 6667;
    static constexpr int MAX_CLIENTS = 1000;
    static constexpr int THREAD_POOL_SIZE = 8;
    
    // [SEQUENCE: 501] Constructor with optional LogBuffer integration
    explicit IRCServer(int port = DEFAULT_IRC_PORT, std::shared_ptr<LogBuffer> logBuffer = nullptr);
    ~IRCServer();
    
    // [SEQUENCE: 502] Server lifecycle management
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // [SEQUENCE: 503] LogBuffer integration for log streaming
    void setLogBuffer(std::shared_ptr<LogBuffer> buffer);
    std::shared_ptr<LogBuffer> getLogBuffer() const { return logBuffer_; }
    
    // [SEQUENCE: 504] Component access for command handlers
    std::shared_ptr<IRCChannelManager> getChannelManager() const { return channelManager_; }
    std::shared_ptr<IRCClientManager> getClientManager() const { return clientManager_; }
    
    // [SEQUENCE: 505] Server information
    std::string getServerName() const { return "logcrafter-irc"; }
    std::string getServerVersion() const { return "1.0"; }
    std::string getServerCreated() const { return serverCreated_; }
    
private:
    // [SEQUENCE: 506] Network setup and client handling
    void setupSocket();
    void acceptClients();
    void handleClient(int clientFd, const std::string& clientAddr);
    void processClientData(int clientFd, const std::string& data);
    
    // [SEQUENCE: 507] Server state
    int port_;
    int listenFd_;
    std::atomic<bool> running_;
    std::thread acceptThread_;
    std::string serverCreated_;
    
    // [SEQUENCE: 508] Core components
    std::shared_ptr<LogBuffer> logBuffer_;
    std::unique_ptr<ThreadPool> threadPool_;
    std::shared_ptr<IRCChannelManager> channelManager_;
    std::shared_ptr<IRCClientManager> clientManager_;
    std::unique_ptr<IRCCommandParser> commandParser_;
    std::unique_ptr<IRCCommandHandler> commandHandler_;
    
    // [SEQUENCE: 509] Thread safety
    mutable std::mutex mutex_;
};