#include "QueryHandler.h"
#include <sstream>
#include <algorithm>
#include <regex>

// [SEQUENCE: 214] Constructor stores buffer reference
QueryHandler::QueryHandler(std::shared_ptr<LogBuffer> buffer) 
    : buffer_(buffer) {
    if (!buffer_) {
        throw std::invalid_argument("LogBuffer cannot be null");
    }
}

// [SEQUENCE: 215] Main query processing method
std::string QueryHandler::processQuery(const std::string& query) {
    // Check for custom handlers first
    for (const auto& [command, handler] : customHandlers_) {
        if (query.find(command) == 0) {
            return handler(query);
        }
    }
    
    // [SEQUENCE: 216] Process built-in queries
    QueryType type = parseQueryType(query);
    
    switch (type) {
        case QueryType::SEARCH:
            return handleSearch(query);
        case QueryType::STATS:
            return handleStats();
        case QueryType::COUNT:
            return handleCount();
        case QueryType::UNKNOWN:
        default:
            return "ERROR: Unknown command\n";
    }
}

// [SEQUENCE: 217] Register custom query handler
void QueryHandler::registerHandler(const std::string& command, QueryCallback handler) {
    customHandlers_[command] = handler;
}

// [SEQUENCE: 218] Parse query type from command string
QueryType QueryHandler::parseQueryType(const std::string& query) const {
    if (query.find("QUERY ") == 0) {
        return QueryType::SEARCH;
    } else if (query == "STATS") {
        return QueryType::STATS;
    } else if (query == "COUNT") {
        return QueryType::COUNT;
    }
    return QueryType::UNKNOWN;
}

// [SEQUENCE: 219] Handle search queries
std::string QueryHandler::handleSearch(const std::string& query) {
    // Extract keyword from "QUERY keyword=xxx"
    std::string keyword = extractParameter(query, "keyword=");
    if (keyword.empty()) {
        // Try alternative format "QUERY xxx"
        size_t pos = query.find(' ');
        if (pos != std::string::npos) {
            keyword = query.substr(pos + 1);
        }
    }
    
    if (keyword.empty()) {
        return "ERROR: No keyword specified\n";
    }
    
    // [SEQUENCE: 220] Search buffer and format results
    auto results = buffer_->search(keyword);
    std::stringstream response;
    response << "FOUND: " << results.size() << " matches\n";
    
    for (const auto& result : results) {
        response << result << "\n";
    }
    
    return response.str();
}

// [SEQUENCE: 221] Handle statistics queries
std::string QueryHandler::handleStats() {
    auto stats = buffer_->getStats();
    size_t current = buffer_->size();
    
    std::stringstream response;
    response << "STATS: "
             << "Total=" << stats.totalLogs
             << ", Dropped=" << stats.droppedLogs
             << ", Current=" << current
             << "\n";
    
    return response.str();
}

// [SEQUENCE: 222] Handle count queries
std::string QueryHandler::handleCount() {
    size_t count = buffer_->size();
    std::stringstream response;
    response << "COUNT: " << count << "\n";
    return response.str();
}

// [SEQUENCE: 223] Extract parameter value from query string
std::string QueryHandler::extractParameter(const std::string& query, 
                                         const std::string& param) const {
    size_t pos = query.find(param);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += param.length();
    size_t end = query.find(' ', pos);
    if (end == std::string::npos) {
        return query.substr(pos);
    }
    
    return query.substr(pos, end - pos);
}