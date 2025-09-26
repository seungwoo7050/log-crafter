#include "query_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

// [SEQUENCE: 245] Create parsed query structure
parsed_query_t* query_parser_create(void) {
    parsed_query_t* query = calloc(1, sizeof(parsed_query_t));
    if (!query) {
        return NULL;
    }
    
    query->time_from = 0;
    query->time_to = 0;
    query->op = OP_AND;  // Default to AND
    query->has_regex = false;
    query->has_time_filter = false;
    
    return query;
}

// [SEQUENCE: 246] Destroy parsed query and free resources
void query_parser_destroy(parsed_query_t* query) {
    if (!query) return;
    
    // Free keywords
    for (int i = 0; i < query->keyword_count; i++) {
        free(query->keywords[i]);
    }
    
    // Free regex pattern
    if (query->regex_pattern) {
        free(query->regex_pattern);
    }
    
    // Free compiled regex
    if (query->compiled_regex) {
        regfree(query->compiled_regex);
        free(query->compiled_regex);
    }
    
    free(query);
}

// [SEQUENCE: 247] Parse query string into structured format
int query_parser_parse(parsed_query_t* query, const char* query_string) {
    if (!query || !query_string) {
        return -1;
    }
    
    // Skip "QUERY " prefix if present
    if (strncmp(query_string, "QUERY ", 6) == 0) {
        query_string += 6;
    }
    
    // [SEQUENCE: 248] Tokenize query string
    char* query_copy = strdup(query_string);
    if (!query_copy) {
        return -1;
    }
    
    char* saveptr1;
    char* token = strtok_r(query_copy, " ", &saveptr1);
    
    while (token) {
        // [SEQUENCE: 249] Parse each parameter
        char* equals = strchr(token, '=');
        if (equals) {
            *equals = '\0';
            char* param = token;
            char* value = equals + 1;
            
            param_type_t type = query_parser_get_param_type(param);
            
            switch (type) {
                case PARAM_KEYWORD: {
                    // [SEQUENCE: 250] Parse comma-separated keywords
                    char* keywords_copy = strdup(value);
                    if (!keywords_copy) {
                        break;
                    }
                    char* saveptr2;
                    char* keyword = strtok_r(keywords_copy, ",", &saveptr2);
                    while (keyword && query->keyword_count < 10) {
                        query->keywords[query->keyword_count++] = strdup(keyword);
                        keyword = strtok_r(NULL, ",", &saveptr2);
                    }
                    free(keywords_copy);
                    break;
                }
                
                case PARAM_REGEX: {
                    // [SEQUENCE: 251] Compile regex pattern
                    query->regex_pattern = strdup(value);
                    if (!query->regex_pattern) {
                        free(query_copy);
                        return -1;
                    }
                    query->compiled_regex = malloc(sizeof(regex_t));
                    if (!query->compiled_regex) {
                        free(query->regex_pattern);
                        query->regex_pattern = NULL;
                        free(query_copy);
                        return -1;
                    }
                    if (regcomp(query->compiled_regex, value, REG_EXTENDED) != 0) {
                        free(query->compiled_regex);
                        query->compiled_regex = NULL;
                        // [SEQUENCE: 407] Fix memory leak - free regex_pattern on failure
                        if (query->regex_pattern) {
                            free(query->regex_pattern);
                            query->regex_pattern = NULL;
                        }
                        free(query_copy);
                        return -1;
                    }
                    query->has_regex = true;
                    break;
                }
                
                case PARAM_TIME_FROM: {
                    // [SEQUENCE: 410] Safe time_t parsing
                    char* endptr;
                    errno = 0;
                    long time_value = strtol(value, &endptr, 10);
                    if (errno != 0 || *endptr != '\0' || time_value < 0) {
                        // Invalid time, skip this parameter
                        break;
                    }
                    query->time_from = (time_t)time_value;
                    query->has_time_filter = true;
                    break;
                }
                    
                case PARAM_TIME_TO: {
                    // [SEQUENCE: 411] Safe time_t parsing
                    char* endptr;
                    errno = 0;
                    long time_value = strtol(value, &endptr, 10);
                    if (errno != 0 || *endptr != '\0' || time_value < 0) {
                        // Invalid time, skip this parameter
                        break;
                    }
                    query->time_to = (time_t)time_value;
                    query->has_time_filter = true;
                    break;
                }
                    
                case PARAM_OPERATOR:
                    query->op = query_parser_parse_operator(value);
                    break;
                    
                default:
                    break;
            }
        }
        
        token = strtok_r(NULL, " ", &saveptr1);
    }
    
    free(query_copy);
    return 0;
}

// [SEQUENCE: 252] Get parameter type from string
param_type_t query_parser_get_param_type(const char* param) {
    if (strcmp(param, "keyword") == 0 || strcmp(param, "keywords") == 0) {
        return PARAM_KEYWORD;
    } else if (strcmp(param, "regex") == 0) {
        return PARAM_REGEX;
    } else if (strcmp(param, "time_from") == 0) {
        return PARAM_TIME_FROM;
    } else if (strcmp(param, "time_to") == 0) {
        return PARAM_TIME_TO;
    } else if (strcmp(param, "operator") == 0) {
        return PARAM_OPERATOR;
    }
    return PARAM_UNKNOWN;
}

// [SEQUENCE: 253] Parse operator string
operator_type_t query_parser_parse_operator(const char* value) {
    if (strcasecmp(value, "AND") == 0) {
        return OP_AND;
    } else if (strcasecmp(value, "OR") == 0) {
        return OP_OR;
    }
    return OP_NONE;
}

// [SEQUENCE: 254] Check if log matches query criteria
bool query_matches_log(const parsed_query_t* query, const char* log_message, time_t timestamp) {
    if (!query || !log_message) {
        return false;
    }
    
    // [SEQUENCE: 255] Check time filter
    if (query->has_time_filter) {
        if (query->time_from > 0 && timestamp < query->time_from) {
            return false;
        }
        if (query->time_to > 0 && timestamp > query->time_to) {
            return false;
        }
    }
    
    // [SEQUENCE: 256] Check regex pattern
    if (query->has_regex && query->compiled_regex) {
        if (regexec(query->compiled_regex, log_message, 0, NULL, 0) != 0) {
            return false;
        }
    }
    
    // [SEQUENCE: 257] Check keywords with operator
    if (query->keyword_count > 0) {
        bool keyword_match = false;
        
        if (query->op == OP_AND) {
            // All keywords must match
            keyword_match = true;
            for (int i = 0; i < query->keyword_count; i++) {
                if (!strstr(log_message, query->keywords[i])) {
                    keyword_match = false;
                    break;
                }
            }
        } else { // OP_OR or default
            // At least one keyword must match (OR)
            for (int i = 0; i < query->keyword_count; i++) {
                if (strstr(log_message, query->keywords[i])) {
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
