#include "IRCServer.h"
#include "IRCChannelManager.h"
#include "IRCClientManager.h"
#include "IRCCommandParser.h"
#include "IRCCommandHandler.h"
#include "IRCClient.h"
#include "ThreadPool.h"
#include "LogBuffer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>

// [SEQUENCE: 829] Constructor
IRCServer::IRCServer(int port, std::shared_ptr<LogBuffer> logBuffer)
    : port_(port)
    , listenFd_(-1)
    , running_(false)
    , logBuffer_(logBuffer) {
    
    // [SEQUENCE: 830] Initialize server creation time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    serverCreated_ = std::ctime(&time_t);
    serverCreated_.pop_back(); // Remove newline
    
    // [SEQUENCE: 831] Create core components
    threadPool_ = std::make_unique<ThreadPool>(THREAD_POOL_SIZE);
    channelManager_ = std::make_shared<IRCChannelManager>();
    clientManager_ = std::make_shared<IRCClientManager>();
    commandParser_ = std::make_unique<IRCCommandParser>();
    commandHandler_ = std::make_unique<IRCCommandHandler>(this);
}

// [SEQUENCE: 832] Destructor
IRCServer::~IRCServer() {
    stop();
}

// [SEQUENCE: 833] Start server
void IRCServer::start() {
    if (running_.load()) {
        return;
    }
    
    try {
        // [SEQUENCE: 834] Setup socket
        setupSocket();
        
        // [SEQUENCE: 835] Initialize log channels
        channelManager_->initializeLogChannels();
        
        // [SEQUENCE: 836] Set up log callback if buffer exists
        if (logBuffer_) {
            logBuffer_->registerChannelCallback("*", 
                [this](const LogEntry& entry) {
                    channelManager_->distributeLogEntry(entry);
                });
        }
        
        running_ = true;
        
        // [SEQUENCE: 837] Start accept thread
        acceptThread_ = std::thread(&IRCServer::acceptClients, this);
        
        std::cout << "IRC server started on port " << port_ << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start IRC server: " << e.what() << std::endl;
        throw;
    }
}

// [SEQUENCE: 838] Stop server
void IRCServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    
    // [SEQUENCE: 839] Close listen socket
    if (listenFd_ != -1) {
        close(listenFd_);
        listenFd_ = -1;
    }
    
    // [SEQUENCE: 840] Wait for accept thread
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    
    // [SEQUENCE: 841] Disconnect all clients
    auto allClients = clientManager_->getAllClients();
    for (const auto& client : allClients) {
        channelManager_->partAllChannels(client, "Server shutting down");
        clientManager_->removeClient(client->getFd());
    }
    
    // [SEQUENCE: 842] Unregister log callback
    if (logBuffer_) {
        logBuffer_->unregisterChannelCallback("*");
    }
    
    std::cout << "IRC server stopped" << std::endl;
}

// [SEQUENCE: 843] Set log buffer
void IRCServer::setLogBuffer(std::shared_ptr<LogBuffer> buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // [SEQUENCE: 844] Unregister old callback
    if (logBuffer_) {
        logBuffer_->unregisterChannelCallback("*");
    }
    
    logBuffer_ = buffer;
    
    // [SEQUENCE: 845] Register new callback
    if (logBuffer_ && running_) {
        logBuffer_->registerChannelCallback("*", 
            [this](const LogEntry& entry) {
                channelManager_->distributeLogEntry(entry);
            });
    }
}

// [SEQUENCE: 846] Setup listening socket
void IRCServer::setupSocket() {
    // [SEQUENCE: 847] Create socket
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }
    
    // [SEQUENCE: 848] Set socket options
    int opt = 1;
    if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(listenFd_);
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)));
    }
    
    // [SEQUENCE: 849] Set non-blocking mode
    int flags = fcntl(listenFd_, F_GETFL, 0);
    if (flags < 0 || fcntl(listenFd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(listenFd_);
        throw std::runtime_error("Failed to set non-blocking mode: " + std::string(strerror(errno)));
    }
    
    // [SEQUENCE: 850] Bind to address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listenFd_);
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
    }
    
    // [SEQUENCE: 851] Start listening
    if (listen(listenFd_, 128) < 0) {
        close(listenFd_);
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
    }
}

// [SEQUENCE: 852] Accept client connections
void IRCServer::acceptClients() {
    while (running_.load()) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        
        // [SEQUENCE: 853] Accept connection
        int clientFd = accept(listenFd_, (struct sockaddr*)&clientAddr, &addrLen);
        
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // [SEQUENCE: 854] No pending connections, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            if (running_.load()) {
                std::cerr << "Accept error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // [SEQUENCE: 855] Get client address
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        std::string clientAddress = std::string(addrStr) + ":" + 
                                   std::to_string(ntohs(clientAddr.sin_port));
        
        // [SEQUENCE: 856] Check connection limits
        if (!clientManager_->canAcceptConnection(addrStr)) {
            std::string errorMsg = "ERROR :Too many connections from your host\r\n";
            send(clientFd, errorMsg.c_str(), errorMsg.length(), MSG_NOSIGNAL);
            close(clientFd);
            continue;
        }
        
        // [SEQUENCE: 857] Set client socket to non-blocking
        int flags = fcntl(clientFd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
        }
        
        // [SEQUENCE: 858] Add client
        auto client = clientManager_->addClient(clientFd, clientAddress);
        if (!client) {
            std::string errorMsg = "ERROR :Server is full\r\n";
            send(clientFd, errorMsg.c_str(), errorMsg.length(), MSG_NOSIGNAL);
            close(clientFd);
            continue;
        }
        
        std::cout << "New IRC client connected from " << clientAddress << std::endl;
        
        // [SEQUENCE: 859] Handle client in thread pool
        threadPool_->enqueue([this, clientFd, clientAddress]() {
            handleClient(clientFd, clientAddress);
        });
    }
}

// [SEQUENCE: 860] Handle client connection
void IRCServer::handleClient(int clientFd, const std::string& clientAddr) {
    char buffer[4096];
    std::string incomplete;
    
    while (running_.load()) {
        // [SEQUENCE: 861] Read data from client
        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            
            // [SEQUENCE: 862] Process received data
            std::string data = incomplete + std::string(buffer, bytesRead);
            incomplete.clear();
            
            // [SEQUENCE: 863] Split by lines
            size_t pos = 0;
            while ((pos = data.find("\r\n")) != std::string::npos || 
                   (pos = data.find("\n")) != std::string::npos) {
                
                std::string line = data.substr(0, pos);
                data.erase(0, pos + (data[pos] == '\r' ? 2 : 1));
                
                if (!line.empty()) {
                    processClientData(clientFd, line);
                }
            }
            
            // [SEQUENCE: 864] Save incomplete line
            if (!data.empty()) {
                incomplete = data;
            }
            
            // [SEQUENCE: 865] Update activity
            clientManager_->updateClientActivity(clientFd);
            
        } else if (bytesRead == 0) {
            // [SEQUENCE: 866] Client disconnected
            break;
        } else {
            // [SEQUENCE: 867] Check for errors
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
            
            // [SEQUENCE: 868] No data available, check if client still exists
            if (!clientManager_->clientExists(clientFd)) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // [SEQUENCE: 869] Clean up disconnected client
    auto client = clientManager_->getClientByFd(clientFd);
    if (client) {
        std::cout << "IRC client disconnected: " << client->getNickname() 
                  << " (" << clientAddr << ")" << std::endl;
        
        // Send QUIT to channels
        IRCCommandParser::IRCCommand quitCmd;
        quitCmd.command = "QUIT";
        quitCmd.trailing = "Connection closed";
        commandHandler_->handleCommand(client, quitCmd);
        
        clientManager_->removeClient(clientFd);
    }
    
    close(clientFd);
}

// [SEQUENCE: 870] Process client data
void IRCServer::processClientData(int clientFd, const std::string& data) {
    auto client = clientManager_->getClientByFd(clientFd);
    if (!client) {
        return;
    }
    
    // [SEQUENCE: 871] Parse IRC command
    auto cmd = IRCCommandParser::parse(data);
    
    if (!cmd.command.empty()) {
        // [SEQUENCE: 872] Handle command
        commandHandler_->handleCommand(client, cmd);
    }
}