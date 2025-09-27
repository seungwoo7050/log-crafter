/*
 * Sequence: SEQ0041
 * Track: C++
 * MVP: mvp3
 * Change: Define the advanced query request model and parsing helpers for keyword, regex, and time filters.
 * Tests: smoke_cpp_mvp3_query
 */
#ifndef LOGCRAFTER_CPP_QUERY_PARSER_HPP
#define LOGCRAFTER_CPP_QUERY_PARSER_HPP

#include <ctime>
#include <regex>
#include <string>
#include <vector>

namespace logcrafter::cpp {

struct QueryRequest {
    enum class Operator {
        And,
        Or,
    };

    std::string keyword;
    std::vector<std::string> keywords;
    Operator keyword_operator = Operator::And;

    bool has_regex = false;
    std::regex regex;

    bool has_time_from = false;
    std::time_t time_from = 0;
    bool has_time_to = false;
    std::time_t time_to = 0;
};

bool parse_query_arguments(const std::string &arguments, QueryRequest &request, std::string &error_message);

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_QUERY_PARSER_HPP
