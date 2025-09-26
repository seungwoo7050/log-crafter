#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <getopt.h>
#include <thread>
#include <signal.h>
#include "LogServer.h"
#include "Logger.h"
#include "Persistence.h"
#include "IRCServer.h"

// [SEQUENCE: 873] Global server pointers for signal handling
static std::shared_ptr<LogServer> g_logServer;
static std::shared_ptr<IRCServer> g_ircServer;

// [SEQUENCE: 874] Signal handler for graceful shutdown
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down servers..." << std::endl;
        if (g_ircServer) g_ircServer->stop();
        if (g_logServer) g_logServer->stop();
        exit(0);
    }
}

// [SEQUENCE: 58] Main entry point for C++ version
int main(int argc, char* argv[]) {
    int port = LogServer::DEFAULT_PORT;
    
    // [SEQUENCE: 875] IRC server options
    bool irc_enabled = false;
    int irc_port = IRCServer::DEFAULT_IRC_PORT;
    
    // [SEQUENCE: 408] MVP4 persistence options
    bool persistence_enabled = false;
    std::string log_directory = "./logs";
    size_t max_file_size = 10 * 1024 * 1024; // 10MB default
    
    // [SEQUENCE: 59] Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:d:s:PiI:h")) != -1) {
        switch (opt) {
            case 'p':
                try {
                    port = std::stoi(optarg);
                    if (port <= 0 || port > 65535) {
                        throw std::invalid_argument("Port out of range");
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Invalid port number: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'P':
                persistence_enabled = true;
                break;
            case 'd':
                log_directory = optarg;
                break;
            case 's':
                try {
                    max_file_size = std::stoul(optarg) * 1024 * 1024; // MB to bytes
                } catch (const std::exception& e) {
                    std::cerr << "Invalid file size: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'i':
                irc_enabled = true;
                break;
            case 'I':
                try {
                    irc_port = std::stoi(optarg);
                    if (irc_port <= 0 || irc_port > 65535) {
                        throw std::invalid_argument("IRC port out of range");
                    }
                    irc_enabled = true;
                } catch (const std::exception& e) {
                    std::cerr << "Invalid IRC port number: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'h':
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -p PORT    Set server port (default: " << LogServer::DEFAULT_PORT << ")" << std::endl;
                std::cout << "  -P         Enable persistence to disk" << std::endl;
                std::cout << "  -d DIR     Set log directory (default: ./logs)" << std::endl;
                std::cout << "  -s SIZE    Set max file size in MB (default: 10)" << std::endl;
                std::cout << "  -i         Enable IRC server (default port: " << IRCServer::DEFAULT_IRC_PORT << ")" << std::endl;
                std::cout << "  -I PORT    Enable IRC server on specific port" << std::endl;
                std::cout << "  -h         Show this help message" << std::endl;
                return 0;
            default:
                std::cerr << "Usage: " << argv[0] << " [-p port] [-P] [-d dir] [-s size] [-i] [-I irc_port] [-h]" << std::endl;
                return 1;
        }
    }
    
    // [SEQUENCE: 876] Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // [SEQUENCE: 60] Create and start server
        auto server = std::make_unique<LogServer>(port);
        g_logServer = std::move(server);
        
        // [SEQUENCE: 409] Configure persistence if enabled
        if (persistence_enabled) {
            PersistenceConfig config;
            config.enabled = true;
            config.log_directory = log_directory;
            config.max_file_size = max_file_size;
            config.max_files = 10;
            config.flush_interval = std::chrono::milliseconds(1000);
            config.load_on_startup = true;
            
            try {
                auto persistence = std::make_unique<PersistenceManager>(config);
                g_logServer->setPersistenceManager(std::move(persistence));
                
                std::cout << "Persistence enabled: directory=" << log_directory 
                         << ", max_file_size=" << (max_file_size / (1024 * 1024)) << "MB" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to initialize persistence: " << e.what() << std::endl;
                return 1;
            }
        }
        
        // [SEQUENCE: 877] Create and start IRC server if enabled
        if (irc_enabled) {
            g_ircServer = std::make_shared<IRCServer>(irc_port, g_logServer->getLogBuffer());
            g_ircServer->start();
            std::cout << "IRC server started on port " << irc_port << std::endl;
        }
        
        std::cout << "LogCrafter-CPP server starting on port " << port << std::endl;
        if (persistence_enabled) {
            std::cout << "Persistence: ENABLED (logs saved to " << log_directory << ")" << std::endl;
        } else {
            std::cout << "Persistence: DISABLED (logs in memory only)" << std::endl;
        }
        if (irc_enabled) {
            std::cout << "IRC: ENABLED (connect with IRC client to port " << irc_port << ")" << std::endl;
        } else {
            std::cout << "IRC: DISABLED (use -i flag to enable)" << std::endl;
        }
        
        g_logServer->start();
        
        // [SEQUENCE: 878] Keep main thread alive
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}