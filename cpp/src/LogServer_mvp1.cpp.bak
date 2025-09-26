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

// [SEQUENCE: 47] Constructor with initialization list
LogServer::LogServer(int port) 
    : port_(port)
    , listenFd_(-1)
    , maxFd_(-1)
    , running_(false)
    , clientCount_(0)
    , logger_(std::make_unique<ConsoleLogger>()) {
    FD_ZERO(&masterSet_);
    FD_ZERO(&readSet_);
}

// [SEQUENCE: 48] Destructor ensures cleanup
LogServer::~LogServer() {
    stop();
    
    // Close all client connections
    for (const auto& client : clients_) {
        close(client.fd);
    }
    
    if (listenFd_ >= 0) {
        close(listenFd_);
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
        ss << "LogCrafter-CPP server starting on port " << port_;
        logger_->info(ss.str());
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

// [SEQUENCE: 52] Initialize server socket
void LogServer::initialize() {
    // Create socket
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }
    
    // Set socket options
    int yes = 1;
    if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)));
    }
    
    // Bind socket
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
    }
    
    // Listen for connections
    if (listen(listenFd_, BACKLOG) < 0) {
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
    }
    
    // Add listener to master set
    FD_SET(listenFd_, &masterSet_);
    maxFd_ = listenFd_;
    
    // Setup signal handler
    g_server = this;
    signal(SIGINT, signalHandler);
}

// [SEQUENCE: 53] Main event loop
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
        
        // Check all file descriptors
        for (int fd = 0; fd <= maxFd_; ++fd) {
            if (!FD_ISSET(fd, &readSet_)) continue;
            
            if (fd == listenFd_) {
                handleNewConnection();
            } else {
                handleClientData(fd);
            }
        }
    }
}

// [SEQUENCE: 54] Handle new connections
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
    
    // Add to master set
    FD_SET(newFd, &masterSet_);
    if (newFd > maxFd_) {
        maxFd_ = newFd;
    }
    
    // Track client info
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        ClientInfo client;
        client.fd = newFd;
        client.address = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + 
                        std::to_string(ntohs(clientAddr.sin_port));
        clients_.push_back(client);
    }
    
    clientCount_++;
    
    std::stringstream ss;
    ss << "New connection from " << inet_ntoa(clientAddr.sin_addr) 
       << ":" << ntohs(clientAddr.sin_port) 
       << " (fd=" << newFd << ", total clients=" << clientCount_ << ")";
    logger_->info(ss.str());
}

// [SEQUENCE: 55] Handle client data
void LogServer::handleClientData(int clientFd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            logger_->info("Client disconnected (fd=" + std::to_string(clientFd) + ")");
        } else {
            logger_->error("Recv error: " + std::string(strerror(errno)));
        }
        closeClient(clientFd);
        return;
    }
    
    // Process log message
    buffer[bytesRead] = '\0';
    
    // Remove trailing newline if present
    if (bytesRead > 0 && buffer[bytesRead - 1] == '\n') {
        buffer[bytesRead - 1] = '\0';
    }
    
    // Log the message
    logger_->log(buffer);
}

// [SEQUENCE: 56] Close client connection
void LogServer::closeClient(int clientFd) {
    FD_CLR(clientFd, &masterSet_);
    close(clientFd);
    
    // Remove from clients list
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                          [clientFd](const ClientInfo& client) {
                              return client.fd == clientFd;
                          }),
            clients_.end()
        );
    }
    
    clientCount_--;
    
    // Update max_fd if necessary
    if (clientFd == maxFd_) {
        updateMaxFd();
    }
}

// [SEQUENCE: 57] Update maximum file descriptor
void LogServer::updateMaxFd() {
    maxFd_ = listenFd_;
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (const auto& client : clients_) {
        if (client.fd > maxFd_) {
            maxFd_ = client.fd;
        }
    }
}