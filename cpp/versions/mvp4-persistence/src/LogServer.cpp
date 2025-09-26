#include "LogServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

// [SEQUENCE: 45] Global server pointer for signal handling
static LogServer* g_server = nullptr;

// [SEQUENCE: 46] Signal handler
static void signalHandler(int sig) {
    (void)sig;
    if (g_server) {
        g_server->stop();
    }
}

// [SEQUENCE: 225] Constructor with MVP2 components initialization
LogServer::LogServer(int port) 
    : port_(port)
    , queryPort_(QUERY_PORT)
    , listenFd_(-1)
    , queryFd_(-1)
    , maxFd_(-1)
    , running_(false)
    , clientCount_(0)
    , logger_(std::make_unique<ConsoleLogger>())
    , threadPool_(std::make_unique<ThreadPool>())
    , logBuffer_(std::make_shared<LogBuffer>())
    , queryHandler_(std::make_unique<QueryHandler>(logBuffer_)) {
    FD_ZERO(&masterSet_);
    FD_ZERO(&readSet_);
}

// [SEQUENCE: 226] Destructor ensures cleanup
LogServer::~LogServer() {
    stop();
    
    // Close all sockets
    if (listenFd_ >= 0) {
        close(listenFd_);
    }
    if (queryFd_ >= 0) {
        close(queryFd_);
    }
}

// [SEQUENCE: 49] Logger injection
void LogServer::setLogger(std::unique_ptr<Logger> logger) {
    if (logger) {
        logger_ = std::move(logger);
    }
}

// [SEQUENCE: 50] Server start method
void LogServer::start() {
    if (running_) {
        logger_->info("Server already running");
        return;
    }
    
    try {
        initialize();
        running_ = true;
        
        std::stringstream ss;
        ss << "LogCrafter-CPP MVP2 server starting on ports " << port_ 
           << " (logs) and " << queryPort_ << " (queries)";
        logger_->info(ss.str());
        logger_->info("Thread pool size: " + std::to_string(threadPool_->size()));
        logger_->info("Press Ctrl+C to stop");
        
        runEventLoop();
    } catch (const std::exception& e) {
        logger_->error(std::string("Server error: ") + e.what());
        stop();
    }
}

// [SEQUENCE: 51] Server stop method
void LogServer::stop() {
    if (running_) {
        running_ = false;
        logger_->info("Shutting down server...");
    }
}

// [SEQUENCE: 227] Create a listener socket
int LogServer::createListener(int port, const std::string& name) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(name + " socket creation failed: " + std::string(strerror(errno)));
    }
    
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        close(fd);
        throw std::runtime_error(name + " setsockopt failed: " + std::string(strerror(errno)));
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        throw std::runtime_error(name + " bind failed: " + std::string(strerror(errno)));
    }
    
    if (listen(fd, BACKLOG) < 0) {
        close(fd);
        throw std::runtime_error(name + " listen failed: " + std::string(strerror(errno)));
    }
    
    logger_->info(name + " listening on port " + std::to_string(port));
    return fd;
}

// [SEQUENCE: 228] Initialize server with both listeners
void LogServer::initialize() {
    // Create log listener
    listenFd_ = createListener(port_, "Log server");
    
    // Create query listener
    queryFd_ = createListener(queryPort_, "Query server");
    
    // Add both to master set
    FD_SET(listenFd_, &masterSet_);
    FD_SET(queryFd_, &masterSet_);
    maxFd_ = std::max(listenFd_, queryFd_);
    
    // Setup signal handler
    g_server = this;
    signal(SIGINT, signalHandler);
}

// [SEQUENCE: 229] Main event loop - MVP2 version
void LogServer::runEventLoop() {
    timeval tv{};
    
    while (running_) {
        readSet_ = masterSet_;
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int selectRet = select(maxFd_ + 1, &readSet_, nullptr, nullptr, &tv);
        
        if (selectRet < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Select error: " + std::string(strerror(errno)));
        }
        
        if (selectRet == 0) continue; // Timeout
        
        // Check for new log connections
        if (FD_ISSET(listenFd_, &readSet_)) {
            handleNewConnection();
        }
        
        // Check for query connections
        if (FD_ISSET(queryFd_, &readSet_)) {
            handleQueryConnection();
        }
    }
}

// [SEQUENCE: 230] Handle new log connections
void LogServer::handleNewConnection() {
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    
    int newFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (newFd < 0) {
        logger_->error("Failed to accept connection: " + std::string(strerror(errno)));
        return;
    }
    
    if (clientCount_ >= MAX_CLIENTS) {
        std::stringstream ss;
        ss << "Maximum clients reached. Rejecting connection from " 
           << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port);
        logger_->info(ss.str());
        close(newFd);
        return;
    }
    
    std::string address = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + 
                         std::to_string(ntohs(clientAddr.sin_port));
    
    // [SEQUENCE: 231] Submit client handler to thread pool
    threadPool_->enqueue([this, newFd, address]() {
        handleClientTask(newFd, address);
    });
    
    clientCount_++;
    
    logger_->info("New connection from " + address + " (total clients=" + 
                 std::to_string(clientCount_) + ")");
}

// [SEQUENCE: 232] Handle query connections
void LogServer::handleQueryConnection() {
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    
    int clientFd = accept(queryFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientFd < 0) {
        logger_->error("Failed to accept query connection: " + std::string(strerror(errno)));
        return;
    }
    
    std::string address = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + 
                         std::to_string(ntohs(clientAddr.sin_port));
    
    logger_->info("Query connection from " + address);
    
    // [SEQUENCE: 233] Handle query in thread pool
    threadPool_->enqueue([this, clientFd]() {
        char buffer[BUFFER_SIZE];
        ssize_t bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            // Remove trailing newline
            if (bytesRead > 0 && buffer[bytesRead - 1] == '\n') {
                buffer[bytesRead - 1] = '\0';
            }
            
            // Process query and send response
            std::string response = queryHandler_->processQuery(buffer);
            send(clientFd, response.c_str(), response.length(), 0);
        }
        
        close(clientFd);
    });
}

// [SEQUENCE: 234] Client task handler (runs in thread pool)
void LogServer::handleClientTask(int clientFd, const std::string& address) {
    logger_->info("Client handler started for " + address);
    
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    
    // [SEQUENCE: 235] Read logs from client
    while ((bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        
        // Remove trailing newline if present
        if (bytesRead > 0 && buffer[bytesRead - 1] == '\n') {
            buffer[bytesRead - 1] = '\0';
        }
        
        // [SEQUENCE: 236] Store log in buffer
        try {
            logBuffer_->push(buffer);
            
            // [SEQUENCE: 406] Write to persistence if enabled
            if (persistence_) {
                persistence_->write(buffer);
            }
        } catch (const std::exception& e) {
            logger_->error("Failed to store log: " + std::string(e.what()));
        }
    }
    
    if (bytesRead == 0) {
        logger_->info("Client disconnected: " + address);
    } else if (bytesRead < 0) {
        logger_->error("Recv error from " + address + ": " + std::string(strerror(errno)));
    }
    
    close(clientFd);
    clientCount_--;
    
    logger_->info("Client handler completed for " + address);
}

// [SEQUENCE: 237] updateMaxFd not used in MVP2
void LogServer::updateMaxFd() {
    // Not needed in MVP2 - only listeners in select
}

// [SEQUENCE: 407] Set persistence manager
void LogServer::setPersistenceManager(std::unique_ptr<PersistenceManager> persistence) {
    persistence_ = std::move(persistence);
}