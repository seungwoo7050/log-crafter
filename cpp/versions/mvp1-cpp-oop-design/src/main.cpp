#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include "LogServer.h"
#include "Logger.h"

// [SEQUENCE: 58] Main entry point for C++ version
int main(int argc, char* argv[]) {
    int port = LogServer::DEFAULT_PORT;
    
    // [SEQUENCE: 59] Parse command line arguments
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > 65535) {
                throw std::invalid_argument("Port out of range");
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
            return 1;
        }
    }
    
    try {
        // [SEQUENCE: 60] Create and start server
        auto server = std::make_unique<LogServer>(port);
        server->start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}