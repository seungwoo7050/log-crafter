/*
 * Sequence: SEQ0011
 * Track: C
 * MVP: mvp3
 * Change: Define the enhanced query request structure and parser helpers for keyword, regex, and time filters.
 * Tests: smoke_query_enhanced
 */
#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include <regex.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LCQueryOperator {
    LC_QUERY_OPERATOR_AND = 0,
    LC_QUERY_OPERATOR_OR = 1
} LCQueryOperator;

typedef struct LCQueryRequest {
    char *keyword;
    char **keywords;
    size_t keyword_count;
    LCQueryOperator keyword_operator;
    int has_regex;
    regex_t regex;
    int regex_compiled;
    int has_time_from;
    time_t time_from;
    int has_time_to;
    time_t time_to;
} LCQueryRequest;

int lc_query_request_init(LCQueryRequest *request);
void lc_query_request_reset(LCQueryRequest *request);
int lc_query_parse_arguments(const char *arguments, LCQueryRequest *request,
                             char *error_message, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif /* QUERY_PARSER_H */
