/*
 * Sequence: SEQ0014
 * Track: C
 * MVP: mvp3
 * Change: Implement parsing for enhanced QUERY arguments including keyword sets, regex, and time ranges.
 * Tests: smoke_query_enhanced
 */
#include "query_parser.h"

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void lc_query_set_error(char *buffer, size_t capacity, const char *message) {
    if (buffer == NULL || capacity == 0) {
        return;
    }
    if (message == NULL) {
        buffer[0] = '\0';
        return;
    }
    snprintf(buffer, capacity, "%s", message);
}

int lc_query_request_init(LCQueryRequest *request) {
    if (request == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(request, 0, sizeof(*request));
    request->keyword_operator = LC_QUERY_OPERATOR_AND;
    return 0;
}

void lc_query_request_reset(LCQueryRequest *request) {
    if (request == NULL) {
        return;
    }

    if (request->keyword != NULL) {
        free(request->keyword);
    }
    request->keyword = NULL;

    if (request->keywords != NULL) {
        for (size_t i = 0; i < request->keyword_count; ++i) {
            free(request->keywords[i]);
        }
        free(request->keywords);
    }
    request->keywords = NULL;
    request->keyword_count = 0;

    if (request->regex_compiled) {
        regfree(&request->regex);
    }
    request->has_regex = 0;
    request->regex_compiled = 0;

    request->has_time_from = 0;
    request->time_from = 0;
    request->has_time_to = 0;
    request->time_to = 0;

    request->keyword_operator = LC_QUERY_OPERATOR_AND;
}

static int lc_query_add_keyword(LCQueryRequest *request, const char *value) {
    if (value == NULL || value[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    char **resized = realloc(request->keywords, (request->keyword_count + 1) * sizeof(char *));
    if (resized == NULL) {
        return -1;
    }
    request->keywords = resized;

    request->keywords[request->keyword_count] = strdup(value);
    if (request->keywords[request->keyword_count] == NULL) {
        return -1;
    }

    request->keyword_count++;
    return 0;
}

static int lc_query_parse_time(const char *value, int *has_time, time_t *time_value) {
    if (value == NULL || value[0] == '\0' || has_time == NULL || time_value == NULL) {
        errno = EINVAL;
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    long long parsed = strtoll(value, &endptr, 10);
    if (errno != 0 || endptr == value || *endptr != '\0' || parsed < 0) {
        errno = EINVAL;
        return -1;
    }

    *has_time = 1;
    *time_value = (time_t)parsed;
    return 0;
}

static int lc_query_parse_keywords(LCQueryRequest *request, const char *value) {
    if (value == NULL || value[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    char *copy = strdup(value);
    if (copy == NULL) {
        return -1;
    }

    char *saveptr = NULL;
    char *token = strtok_r(copy, ",", &saveptr);
    while (token != NULL) {
        if (token[0] == '\0') {
            free(copy);
            errno = EINVAL;
            return -1;
        }
        if (lc_query_add_keyword(request, token) != 0) {
            free(copy);
            return -1;
        }
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(copy);
    if (request->keyword_count == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int lc_query_compile_regex(LCQueryRequest *request, const char *pattern,
                                  char *error_message, size_t error_capacity) {
    if (pattern == NULL || pattern[0] == '\0') {
        errno = EINVAL;
        lc_query_set_error(error_message, error_capacity, "ERROR: Empty regex parameter.");
        return -1;
    }

    int rc = regcomp(&request->regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
        char buffer[128];
        regerror(rc, &request->regex, buffer, sizeof(buffer));
        char message[192];
        snprintf(message, sizeof(message), "ERROR: Regex compile failed: %s", buffer);
        lc_query_set_error(error_message, error_capacity, message);
        errno = EINVAL;
        return -1;
    }

    request->has_regex = 1;
    request->regex_compiled = 1;
    return 0;
}

int lc_query_parse_arguments(const char *arguments, LCQueryRequest *request,
                             char *error_message, size_t error_capacity) {
    if (request == NULL) {
        errno = EINVAL;
        lc_query_set_error(error_message, error_capacity, "ERROR: Internal parser error.");
        return -1;
    }

    lc_query_set_error(error_message, error_capacity, NULL);

    if (arguments == NULL) {
        lc_query_set_error(error_message, error_capacity, "ERROR: Missing query parameters.");
        errno = EINVAL;
        return -1;
    }

    const char *cursor = arguments;
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (*cursor == '\0') {
        lc_query_set_error(error_message, error_capacity, "ERROR: Missing query parameters.");
        errno = EINVAL;
        return -1;
    }

    char *copy = strdup(cursor);
    if (copy == NULL) {
        lc_query_set_error(error_message, error_capacity, "ERROR: Out of memory.");
        return -1;
    }

    int operator_explicit = 0;

    char *saveptr = NULL;
    char *token = strtok_r(copy, " ", &saveptr);
    while (token != NULL) {
        if (strncmp(token, "keyword=", 8) == 0) {
            if (request->keyword != NULL) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Duplicate keyword parameter.");
                goto error;
            }
            const char *value = token + 8;
            if (value[0] == '\0') {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Empty keyword parameter.");
                errno = EINVAL;
                goto error;
            }
            request->keyword = strdup(value);
            if (request->keyword == NULL) {
                lc_query_set_error(error_message, error_capacity, "ERROR: Out of memory.");
                goto error;
            }
        } else if (strncmp(token, "keywords=", 9) == 0) {
            if (request->keyword_count > 0) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Duplicate keywords parameter.");
                goto error;
            }
            const char *value = token + 9;
            if (lc_query_parse_keywords(request, value) != 0) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Invalid keywords parameter.");
                goto error;
            }
        } else if (strncmp(token, "operator=", 9) == 0) {
            const char *value = token + 9;
            if (value[0] == '\0') {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Empty operator parameter.");
                errno = EINVAL;
                goto error;
            }
            if (strcasecmp(value, "AND") == 0) {
                request->keyword_operator = LC_QUERY_OPERATOR_AND;
            } else if (strcasecmp(value, "OR") == 0) {
                request->keyword_operator = LC_QUERY_OPERATOR_OR;
            } else {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Operator must be AND or OR.");
                errno = EINVAL;
                goto error;
            }
            operator_explicit = 1;
        } else if (strncmp(token, "regex=", 6) == 0) {
            if (request->has_regex) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Duplicate regex parameter.");
                goto error;
            }
            const char *pattern = token + 6;
            if (lc_query_compile_regex(request, pattern, error_message, error_capacity) != 0) {
                goto error;
            }
        } else if (strncmp(token, "time_from=", 10) == 0) {
            if (lc_query_parse_time(token + 10, &request->has_time_from, &request->time_from) != 0) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Invalid time_from parameter.");
                goto error;
            }
        } else if (strncmp(token, "time_to=", 8) == 0) {
            if (lc_query_parse_time(token + 8, &request->has_time_to, &request->time_to) != 0) {
                lc_query_set_error(error_message, error_capacity,
                                   "ERROR: Invalid time_to parameter.");
                goto error;
            }
        } else if (token[0] != '\0') {
            lc_query_set_error(error_message, error_capacity,
                               "ERROR: Unknown query parameter.");
            errno = EINVAL;
            goto error;
        }

        token = strtok_r(NULL, " ", &saveptr);
    }

    free(copy);
    copy = NULL;

    if (operator_explicit && request->keyword_count == 0) {
        lc_query_set_error(error_message, error_capacity,
                           "ERROR: operator requires keywords parameter.");
        errno = EINVAL;
        goto reset_error;
    }

    if (request->keyword == NULL && request->keyword_count == 0 && !request->has_regex &&
        !request->has_time_from && !request->has_time_to) {
        lc_query_set_error(error_message, error_capacity,
                           "ERROR: Provide at least one filter parameter.");
        errno = EINVAL;
        goto reset_error;
    }

    if (request->has_time_from && request->has_time_to && request->time_from > request->time_to) {
        lc_query_set_error(error_message, error_capacity,
                           "ERROR: time_from must be <= time_to.");
        errno = EINVAL;
        goto reset_error;
    }

    return 0;

error:
    free(copy);
reset_error:
    lc_query_request_reset(request);
    lc_query_request_init(request);
    return -1;
}
