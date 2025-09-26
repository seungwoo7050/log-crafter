#include "QueryParser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// [SEQUENCE: 277] Initialize parameter type mapping
const std::unordered_map<std::string, ParamType> QueryParser::param_types = {
    {"keyword", ParamType::KEYWORD},
    {"keywords", ParamType::KEYWORDS},
    {"regex", ParamType::REGEX},
    {"time_from", ParamType::TIME_FROM},
    {"time_to", ParamType::TIME_TO},
    {"operator", ParamType::OPERATOR}
};

// [SEQUENCE: 278] Get parameter type from string
ParamType QueryParser::getParamType(const std::string& param) {
    auto it = param_types.find(param);
    if (it != param_types.end()) {
        return it->second;
    }
    throw std::invalid_argument("Unknown parameter: " + param);
}

// [SEQUENCE: 279] Split string by delimiter
std::vector<std::string> QueryParser::splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

// [SEQUENCE: 280] Parse operator string
OperatorType QueryParser::parseOperator(const std::string& value) {
    std::string upper_value = value;
    std::transform(upper_value.begin(), upper_value.end(), upper_value.begin(), ::toupper);
    
    if (upper_value == "OR") {
        return OperatorType::OR;
    }
    return OperatorType::AND;
}

// [SEQUENCE: 281] Parse query string using modern C++
std::unique_ptr<ParsedQuery> QueryParser::parse(const std::string& query_string) {
    auto query = std::make_unique<ParsedQuery>();
    
    // Skip "QUERY " prefix if present
    std::string params = query_string;
    if (params.substr(0, 6) == "QUERY ") {
        params = params.substr(6);
    }
    
    // [SEQUENCE: 282] Tokenize parameters
    std::istringstream iss(params);
    std::string token;
    
    while (iss >> token) {
        // [SEQUENCE: 283] Parse parameter=value pairs
        size_t equals_pos = token.find('=');
        if (equals_pos != std::string::npos) {
            std::string param = token.substr(0, equals_pos);
            std::string value = token.substr(equals_pos + 1);
            
            try {
                ParamType type = getParamType(param);
                
                switch (type) {
                    case ParamType::KEYWORD:
                        // [SEQUENCE: 284] Handle single keyword (backward compatibility)
                        query->keywords.push_back(value);
                        break;
                        
                    case ParamType::KEYWORDS:
                        // [SEQUENCE: 285] Parse comma-separated keywords
                        {
                            auto keywords = splitString(value, ',');
                            query->keywords.insert(query->keywords.end(), 
                                                 keywords.begin(), keywords.end());
                        }
                        break;
                        
                    case ParamType::REGEX:
                        // [SEQUENCE: 286] Compile regex pattern
                        query->regex_pattern = value;
                        query->compiled_regex = std::regex(value, std::regex::extended);
                        break;
                        
                    case ParamType::TIME_FROM:
                        // [SEQUENCE: 287] Parse start time
                        {
                            auto time_t_value = std::stol(value);
                            query->time_from = std::chrono::system_clock::from_time_t(time_t_value);
                        }
                        break;
                        
                    case ParamType::TIME_TO:
                        // [SEQUENCE: 288] Parse end time
                        {
                            auto time_t_value = std::stol(value);
                            query->time_to = std::chrono::system_clock::from_time_t(time_t_value);
                        }
                        break;
                        
                    case ParamType::OPERATOR:
                        // [SEQUENCE: 289] Parse operator type
                        query->op = parseOperator(value);
                        break;
                }
            } catch (const std::exception& e) {
                // Ignore invalid parameters
            }
        }
    }
    
    return query;
}

// [SEQUENCE: 290] Check if log matches query criteria
bool ParsedQuery::matches(const std::string& log_message, 
                         const std::chrono::system_clock::time_point& timestamp) const {
    // [SEQUENCE: 291] Check time filter
    if (time_from.has_value() && timestamp < time_from.value()) {
        return false;
    }
    if (time_to.has_value() && timestamp > time_to.value()) {
        return false;
    }
    
    // [SEQUENCE: 292] Check regex pattern
    if (compiled_regex.has_value()) {
        if (!std::regex_search(log_message, compiled_regex.value())) {
            return false;
        }
    }
    
    // [SEQUENCE: 293] Check keywords with operator
    if (!keywords.empty()) {
        bool keyword_match = false;
        
        if (op == OperatorType::AND) {
            // All keywords must match
            keyword_match = true;
            for (const auto& keyword : keywords) {
                if (log_message.find(keyword) == std::string::npos) {
                    keyword_match = false;
                    break;
                }
            }
        } else {
            // At least one keyword must match
            for (const auto& keyword : keywords) {
                if (log_message.find(keyword) != std::string::npos) {
                    keyword_match = true;
                    break;
                }
            }
        }
        
        if (!keyword_match) {
            return false;
        }
    }
    
    return true;
}