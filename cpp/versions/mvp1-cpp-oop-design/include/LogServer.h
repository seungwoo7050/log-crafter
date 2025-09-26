#ifndef LOGSERVER_H
#define LOGSERVER_H

#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <sys/select.h>
#include "Logger.h"

// [SEQUENCE: 35] Main server class using RAII
class LogServer {
public:
    static constexpr int DEFAULT_PORT = 9999;
    static constexpr int MAX_CLIENTS = 1024;
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr int BACKLOG = 10;

    // [SEQUENCE: 36] Constructor and destructor for RAII
    explicit LogServer(int port = DEFAULT_PORT);
    ~LogServer();

    // [SEQUENCE: 37] Deleted copy operations for safety
    LogServer(const LogServer&) = delete;
    LogServer& operator=(const LogServer&) = delete;

    // [SEQUENCE: 38] Server lifecycle methods
    void start();
    void stop();
    bool isRunning() const { return running_; }

    // [SEQUENCE: 39] Logger injection for flexibility
    void setLogger(std::unique_ptr<Logger> logger);

private:
    // [SEQUENCE: 40] Private implementation methods
    void initialize();
    void runEventLoop();
    void handleNewConnection();
    void handleClientData(int clientFd);
    void closeClient(int clientFd);
    void updateMaxFd();

    // [SEQUENCE: 41] Member variables
    int port_;
    int listenFd_;
    fd_set masterSet_;
    fd_set readSet_;
    int maxFd_;
    std::atomic<bool> running_;
    std::atomic<int> clientCount_;
    std::unique_ptr<Logger> logger_;
    
    // [SEQUENCE: 42] Client information tracking
    struct ClientInfo {
        int fd;
        std::string address;
    };
    std::vector<ClientInfo> clients_;
    mutable std::mutex clientsMutex_;
};

#endif // LOGSERVER_H