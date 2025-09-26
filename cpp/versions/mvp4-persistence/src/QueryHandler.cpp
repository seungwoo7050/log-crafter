#include "QueryHandler.h"
#include "QueryParser.h"
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
        case QueryType::HELP:
            return handleHelp();
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
    } else if (query == "HELP") {
        return QueryType::HELP;
    }
    return QueryType::UNKNOWN;
}

// [SEQUENCE: 219] Handle search queries
std::string QueryHandler::handleSearch(const std::string& query) {
    try {
        // [SEQUENCE: 298] Parse enhanced query using QueryParser
        auto parsed_query = QueryParser::parse(query);
        
        // [SEQUENCE: 299] Check for simple keyword for backward compatibility
        if (parsed_query->keywords.empty() && 
            !parsed_query->compiled_regex.has_value() && 
            !parsed_query->time_from.has_value() && 
            !parsed_query->time_to.has_value()) {
            
            // Try to extract simple keyword for backward compatibility
            std::string keyword = extractParameter(query, "keyword=");
            if (keyword.empty()) {
                size_t pos = query.find(' ');
                if (pos != std::string::npos) {
                    keyword = query.substr(pos + 1);
                }
            }
            
            if (!keyword.empty()) {
                // Use simple search for backward compatibility
                auto results = buffer_->search(keyword);
                std::stringstream response;
                response << "FOUND: " << results.size() << " matches\n";
                for (const auto& result : results) {
                    response << result << "\n";
                }
                return response.str();
            }
            
            return "ERROR: No search criteria specified\n";
        }
        
        // [SEQUENCE: 300] Use enhanced search with parsed query
        auto results = buffer_->searchEnhanced(*parsed_query);
        std::stringstream response;
        response << "FOUND: " << results.size() << " matches\n";
        
        for (const auto& result : results) {
            response << result << "\n";
        }
        
        return response.str();
        
    } catch (const std::exception& e) {
        return std::string("ERROR: ") + e.what() + "\n";
    }
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

// [SEQUENCE: 301] Handle help queries
std::string QueryHandler::handleHelp() {
    std::stringstream response;
    response << "Available commands:\n"
             << "  STATS - Show buffer statistics\n"
             << "  COUNT - Show number of logs in buffer\n"
             << "  HELP  - Show this help message\n"
             << "  QUERY <parameters> - Search logs with parameters:\n"
             << "\n"
             << "Query parameters:\n"
             << "  keyword=<word>       - Single keyword search (backward compatible)\n"
             << "  keywords=<w1,w2,..> - Multiple keywords (comma-separated)\n"
             << "  operator=<AND|OR>   - Keyword matching logic (default: AND)\n"
             << "  regex=<pattern>     - Regular expression pattern\n"
             << "  time_from=<unix_ts> - Start time (Unix timestamp)\n"
             << "  time_to=<unix_ts>   - End time (Unix timestamp)\n"
             << "\n"
             << "Examples:\n"
             << "  QUERY keyword=error\n"
             << "  QUERY keywords=error,warning operator=OR\n"
             << "  QUERY regex=ERROR.*timeout\n"
             << "  QUERY time_from=1706140800 time_to=1706227200\n"
             << "  QUERY keywords=error,timeout operator=AND regex=.*failed.*\n";
             
    return response.str();
}