#ifndef QUERYHANDLER_H
#define QUERYHANDLER_H

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "LogBuffer.h"

// [SEQUENCE: 205] Query command types
enum class QueryType {
    SEARCH,
    STATS,
    COUNT,
    UNKNOWN
};

// [SEQUENCE: 206] Query handler for processing client queries
class QueryHandler {
public:
    // [SEQUENCE: 207] Constructor with buffer reference
    explicit QueryHandler(std::shared_ptr<LogBuffer> buffer);
    
    // [SEQUENCE: 208] Process query and return response
    std::string processQuery(const std::string& query);
    
    // [SEQUENCE: 209] Register custom query handler
    using QueryCallback = std::function<std::string(const std::string&)>;
    void registerHandler(const std::string& command, QueryCallback handler);

private:
    // [SEQUENCE: 210] Parse query type from command
    QueryType parseQueryType(const std::string& query) const;
    
    // [SEQUENCE: 211] Individual query handlers
    std::string handleSearch(const std::string& query);
    std::string handleStats();
    std::string handleCount();
    
    // [SEQUENCE: 212] Extract parameter from query
    std::string extractParameter(const std::string& query, const std::string& param) const;
    
    // [SEQUENCE: 213] Member variables
    std::shared_ptr<LogBuffer> buffer_;
    std::unordered_map<std::string, QueryCallback> customHandlers_;
};

#endif // QUERYHANDLER_H