#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// [SEQUENCE: 43] Console logger implementation
void ConsoleLogger::log(const std::string& message) {
    writeLog("LOG", message);
}

void ConsoleLogger::info(const std::string& message) {
    writeLog("INFO", message);
}

void ConsoleLogger::error(const std::string& message) {
    writeLog("ERROR", message);
}

// [SEQUENCE: 44] Timestamp formatting for logs
void ConsoleLogger::writeLog(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S") << "] ";
    ss << "[" << level << "] " << message;
    
    std::cout << ss.str() << std::endl;
}