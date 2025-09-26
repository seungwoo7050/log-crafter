#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <getopt.h>
#include "LogServer.h"
#include "Logger.h"
#include "Persistence.h"

// [SEQUENCE: 58] Main entry point for C++ version
int main(int argc, char* argv[]) {
    int port = LogServer::DEFAULT_PORT;
    
    // [SEQUENCE: 408] MVP4 persistence options
    bool persistence_enabled = false;
    std::string log_directory = "./logs";
    size_t max_file_size = 10 * 1024 * 1024; // 10MB default
    
    // [SEQUENCE: 59] Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:d:s:Ph")) != -1) {
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
            case 'h':
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -p PORT    Set server port (default: " << LogServer::DEFAULT_PORT << ")" << std::endl;
                std::cout << "  -P         Enable persistence to disk" << std::endl;
                std::cout << "  -d DIR     Set log directory (default: ./logs)" << std::endl;
                std::cout << "  -s SIZE    Set max file size in MB (default: 10)" << std::endl;
                std::cout << "  -h         Show this help message" << std::endl;
                return 0;
            default:
                std::cerr << "Usage: " << argv[0] << " [-p port] [-P] [-d dir] [-s size] [-h]" << std::endl;
                return 1;
        }
    }
    
    try {
        // [SEQUENCE: 60] Create and start server
        auto server = std::make_unique<LogServer>(port);
        
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
                server->setPersistenceManager(std::move(persistence));
                
                std::cout << "Persistence enabled: directory=" << log_directory 
                         << ", max_file_size=" << (max_file_size / (1024 * 1024)) << "MB" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to initialize persistence: " << e.what() << std::endl;
                return 1;
            }
        }
        
        std::cout << "LogCrafter-CPP server starting on port " << port << std::endl;
        if (persistence_enabled) {
            std::cout << "Persistence: ENABLED (logs saved to " << log_directory << ")" << std::endl;
        } else {
            std::cout << "Persistence: DISABLED (logs in memory only)" << std::endl;
        }
        
        server->start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}