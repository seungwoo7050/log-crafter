#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include <stdbool.h>
#include <time.h>
#include <regex.h>

// [SEQUENCE: 239] Query parameter types
typedef enum {
    PARAM_KEYWORD,
    PARAM_REGEX,
    PARAM_TIME_FROM,
    PARAM_TIME_TO,
    PARAM_OPERATOR,
    PARAM_UNKNOWN
} param_type_t;

// [SEQUENCE: 240] Query operator types
typedef enum {
    OP_AND,
    OP_OR,
    OP_NONE
} operator_type_t;

// [SEQUENCE: 241] Parsed query structure
typedef struct {
    char* keywords[10];          // Multiple keywords
    int keyword_count;
    char* regex_pattern;         // Regex pattern string
    regex_t* compiled_regex;     // Compiled regex
    time_t time_from;           // Start time filter
    time_t time_to;             // End time filter
    operator_type_t op;         // AND/OR operator
    bool has_regex;
    bool has_time_filter;
} parsed_query_t;

// [SEQUENCE: 242] Query parser functions
parsed_query_t* query_parser_create(void);
void query_parser_destroy(parsed_query_t* query);
int query_parser_parse(parsed_query_t* query, const char* query_string);

// [SEQUENCE: 243] Parameter extraction helpers
param_type_t query_parser_get_param_type(const char* param);
char* query_parser_extract_value(const char* param);
operator_type_t query_parser_parse_operator(const char* value);

// [SEQUENCE: 244] Query matching functions
bool query_matches_log(const parsed_query_t* query, const char* log_message, time_t timestamp);

#endif // QUERY_PARSER_H
