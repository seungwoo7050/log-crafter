/*
 * Sequence: SEQ0041
 * Track: C++
 * MVP: mvp3
 * Change: Implement parsing for advanced QUERY arguments including keywords, regex filters, and time windows.
 * Tests: smoke_cpp_mvp3_query
 */
#include "query_parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace logcrafter::cpp {

namespace {

void set_error(std::string &error, const std::string &message) {
    if (message.empty()) {
        error = "ERROR: Invalid query syntax.";
    } else if (message.rfind("ERROR:", 0) == 0) {
        error = message;
    } else {
        error = "ERROR: " + message;
    }
}

bool parse_time(const std::string &value, const char *label, bool &has_flag, std::time_t &out, std::string &error) {
    if (value.empty()) {
        set_error(error, std::string("Invalid ") + label + " parameter.");
        return false;
    }

    std::size_t consumed = 0;
    long long parsed = 0;
    try {
        parsed = std::stoll(value, &consumed, 10);
    } catch (const std::exception &) {
        set_error(error, std::string("Invalid ") + label + " parameter.");
        return false;
    }

    if (consumed != value.size() || parsed < 0) {
        set_error(error, std::string("Invalid ") + label + " parameter.");
        return false;
    }

    has_flag = true;
    out = static_cast<std::time_t>(parsed);
    return true;
}

bool parse_keywords(const std::string &value, std::vector<std::string> &keywords, std::string &error) {
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            set_error(error, "Invalid keywords parameter.");
            return false;
        }
        keywords.push_back(token);
    }

    if (keywords.empty()) {
        set_error(error, "Invalid keywords parameter.");
        return false;
    }

    return true;
}

std::vector<std::string> tokenize(const std::string &input) {
    std::vector<std::string> tokens;
    std::istringstream stream(input);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace

bool parse_query_arguments(const std::string &arguments, QueryRequest &request, std::string &error_message) {
    request = QueryRequest{};
    error_message.clear();

    if (arguments.empty()) {
        set_error(error_message, "Missing query parameters.");
        return false;
    }

    std::size_t first_non_space = arguments.find_first_not_of(" \t\r\n");
    if (first_non_space == std::string::npos) {
        set_error(error_message, "Missing query parameters.");
        return false;
    }

    const std::string trimmed = arguments.substr(first_non_space);
    std::vector<std::string> tokens = tokenize(trimmed);
    if (tokens.empty()) {
        set_error(error_message, "Missing query parameters.");
        return false;
    }

    bool operator_explicit = false;

    for (const std::string &token : tokens) {
        std::size_t equals = token.find('=');
        if (equals == std::string::npos) {
            set_error(error_message, "Unknown query parameter.");
            return false;
        }

        const std::string key = token.substr(0, equals);
        const std::string value = token.substr(equals + 1);

        if (key == "keyword") {
            if (!request.keyword.empty()) {
                set_error(error_message, "Duplicate keyword parameter.");
                return false;
            }
            if (value.empty()) {
                set_error(error_message, "Empty keyword parameter.");
                return false;
            }
            request.keyword = value;
        } else if (key == "keywords") {
            if (!request.keywords.empty()) {
                set_error(error_message, "Duplicate keywords parameter.");
                return false;
            }
            if (!parse_keywords(value, request.keywords, error_message)) {
                return false;
            }
        } else if (key == "operator") {
            if (value.empty()) {
                set_error(error_message, "Empty operator parameter.");
                return false;
            }
            std::string normalized = value;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            if (normalized == "AND") {
                request.keyword_operator = QueryRequest::Operator::And;
            } else if (normalized == "OR") {
                request.keyword_operator = QueryRequest::Operator::Or;
            } else {
                set_error(error_message, "Operator must be AND or OR.");
                return false;
            }
            operator_explicit = true;
        } else if (key == "regex") {
            if (request.has_regex) {
                set_error(error_message, "Duplicate regex parameter.");
                return false;
            }
            if (value.empty()) {
                set_error(error_message, "Empty regex parameter.");
                return false;
            }
            try {
                request.regex = std::regex(value, std::regex_constants::extended);
            } catch (const std::regex_error &ex) {
                set_error(error_message, std::string("Regex compile failed: ") + ex.what());
                return false;
            }
            request.has_regex = true;
        } else if (key == "time_from") {
            if (!parse_time(value, "time_from", request.has_time_from, request.time_from, error_message)) {
                return false;
            }
        } else if (key == "time_to") {
            if (!parse_time(value, "time_to", request.has_time_to, request.time_to, error_message)) {
                return false;
            }
        } else {
            set_error(error_message, "Unknown query parameter.");
            return false;
        }
    }

    if (operator_explicit && request.keywords.empty()) {
        set_error(error_message, "operator requires keywords parameter.");
        return false;
    }

    if (request.keyword.empty() && request.keywords.empty() && !request.has_regex &&
        !request.has_time_from && !request.has_time_to) {
        set_error(error_message, "Provide at least one filter parameter.");
        return false;
    }

    if (request.has_time_from && request.has_time_to && request.time_from > request.time_to) {
        set_error(error_message, "time_from must be <= time_to.");
        return false;
    }

    error_message.clear();
    return true;
}

} // namespace logcrafter::cpp
