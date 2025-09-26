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
#include "ThreadPool.h"
#include "LogBuffer.h"
#include "QueryHandler.h"
#include "Persistence.h"

// [SEQUENCE: 35] Main server class using RAII - MVP2 version
class LogServer {
public:
    static constexpr int DEFAULT_PORT = 9999;
    static constexpr int QUERY_PORT = 9998;
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
    
    // [SEQUENCE: 404] Set persistence manager
    void setPersistenceManager(std::unique_ptr<PersistenceManager> persistence);

private:
    // [SEQUENCE: 40] Private implementation methods
    void initialize();
    void runEventLoop();
    void handleNewConnection();
    void handleQueryConnection();
    void handleClientTask(int clientFd, const std::string& address);
    void updateMaxFd();
    int createListener(int port, const std::string& name);

    // [SEQUENCE: 41] Member variables - MVP2 additions
    int port_;
    int queryPort_;
    int listenFd_;
    int queryFd_;
    fd_set masterSet_;
    fd_set readSet_;
    int maxFd_;
    std::atomic<bool> running_;
    std::atomic<int> clientCount_;
    std::unique_ptr<Logger> logger_;
    
    // [SEQUENCE: 224] MVP2 components
    std::unique_ptr<ThreadPool> threadPool_;
    std::shared_ptr<LogBuffer> logBuffer_;
    std::unique_ptr<QueryHandler> queryHandler_;
    
    // [SEQUENCE: 405] MVP4 persistence
    std::unique_ptr<PersistenceManager> persistence_;
};

#endif // LOGSERVER_H