/*
 * Sequence: SEQ0028
 * Track: C
 * MVP: mvp5
 * Change: Clamp stored messages to a fixed length, scrub unsafe characters, and preserve timestamp replay semantics.
 * Tests: smoke_security_capacity
 */
#include "log_buffer.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void lc_log_buffer_scrub_message(char *message) {
    if (message == NULL) {
        return;
    }

    for (char *cursor = message; *cursor != '\0'; ++cursor) {
        unsigned char ch = (unsigned char)*cursor;
        if (ch == '\t' || ch == ' ') {
            continue;
        }
        if (!isprint(ch)) {
            *cursor = '?';
        }
    }
}

static char *lc_log_buffer_clone_message(const char *message) {
    if (message == NULL) {
        return NULL;
    }

    size_t limit = LC_LOG_BUFFER_MAX_MESSAGE_LENGTH;
    size_t length = strnlen(message, limit + 1);
    int truncated = 0;
    if (length > limit) {
        length = limit;
        truncated = 1;
    }

    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, message, length);
    copy[length] = '\0';

    if (truncated && length >= 3) {
        memcpy(copy + length - 3, "...", 3);
    }

    lc_log_buffer_scrub_message(copy);
    return copy;
}

static void lc_log_entry_clear(LCLogEntry *entry) {
    if (entry == NULL) {
        return;
    }
    if (entry->message != NULL) {
        free(entry->message);
        entry->message = NULL;
    }
    entry->timestamp = 0;
}

static int lc_log_buffer_append_owned(char ***results, size_t *count, size_t *capacity,
                                      char *value) {
    if (*count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        char **resized = realloc(*results, new_capacity * sizeof(char *));
        if (resized == NULL) {
            return -1;
        }
        *results = resized;
        *capacity = new_capacity;
    }

    (*results)[*count] = value;
    (*count)++;
    return 0;
}

static int lc_log_buffer_append_match(char ***results, size_t *count, size_t *capacity,
                                      const char *message) {
    if (*count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        char **resized = realloc(*results, new_capacity * sizeof(char *));
        if (resized == NULL) {
            return -1;
        }
        *results = resized;
        *capacity = new_capacity;
    }

    (*results)[*count] = strdup(message);
    if ((*results)[*count] == NULL) {
        return -1;
    }

    (*count)++;
    return 0;
}

static int lc_safe_localtime(time_t timestamp, struct tm *out) {
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    if (localtime_r(&timestamp, out) == NULL) {
        return -1;
    }
    return 0;
#else
    struct tm *tmp = localtime(&timestamp);
    if (tmp == NULL) {
        return -1;
    }
    *out = *tmp;
    return 0;
#endif
}

static void lc_format_timestamp(time_t timestamp, char *buffer, size_t capacity) {
    struct tm tm_value;
    if (lc_safe_localtime(timestamp, &tm_value) != 0) {
        memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = 70;  /* 1970 */
        tm_value.tm_mon = 0;
        tm_value.tm_mday = 1;
    }

    if (strftime(buffer, capacity, "%Y-%m-%d %H:%M:%S", &tm_value) == 0) {
        snprintf(buffer, capacity, "1970-01-01 00:00:00");
    }
}

static char *lc_log_buffer_format_entry(const LCLogEntry *entry) {
    if (entry == NULL || entry->message == NULL) {
        return NULL;
    }

    char timestamp[32];
    lc_format_timestamp(entry->timestamp, timestamp, sizeof(timestamp));

    size_t message_length = strlen(entry->message);
    size_t total_length = strlen(timestamp) + message_length + 4; /* [ts] + space + null */
    char *formatted = malloc(total_length);
    if (formatted == NULL) {
        return NULL;
    }

    int written = snprintf(formatted, total_length, "[%s] %s", timestamp, entry->message);
    if (written < 0) {
        free(formatted);
        return NULL;
    }

    return formatted;
}

int lc_log_buffer_init(LCLogBuffer *buffer, size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        errno = EINVAL;
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->capacity = capacity;
    buffer->entries = calloc(capacity, sizeof(LCLogEntry));
    if (buffer->entries == NULL) {
        return -1;
    }

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->entries);
        buffer->entries = NULL;
        return -1;
    }

    return 0;
}

void lc_log_buffer_destroy(LCLogBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    if (buffer->entries != NULL) {
        for (size_t i = 0; i < buffer->capacity; ++i) {
            lc_log_entry_clear(&buffer->entries[i]);
        }
        free(buffer->entries);
        buffer->entries = NULL;
    }

    if (buffer->capacity != 0) {
        pthread_mutex_destroy(&buffer->mutex);
    }

    buffer->capacity = 0;
    buffer->size = 0;
    buffer->head = 0;
    buffer->total_logs = 0;
    buffer->dropped_logs = 0;
}

static size_t lc_log_buffer_oldest_index(const LCLogBuffer *buffer) {
    if (buffer->size == 0) {
        return 0;
    }
    if (buffer->head >= buffer->size) {
        return buffer->head - buffer->size;
    }
    return buffer->capacity - (buffer->size - buffer->head);
}

int lc_log_buffer_push_with_time(LCLogBuffer *buffer, const char *message, time_t timestamp) {
    if (buffer == NULL || message == NULL) {
        errno = EINVAL;
        return -1;
    }

    char *copy = lc_log_buffer_clone_message(message);
    if (copy == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&buffer->mutex) != 0) {
        free(copy);
        return -1;
    }

    LCLogEntry *slot = &buffer->entries[buffer->head];
    if (buffer->size == buffer->capacity && slot->message != NULL) {
        lc_log_entry_clear(slot);
        buffer->dropped_logs++;
    }

    slot->message = copy;
    slot->timestamp = timestamp;

    buffer->head = (buffer->head + 1) % buffer->capacity;
    if (buffer->size < buffer->capacity) {
        buffer->size++;
    }
    buffer->total_logs++;

    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int lc_log_buffer_push(LCLogBuffer *buffer, const char *message) {
    return lc_log_buffer_push_with_time(buffer, message, time(NULL));
}

size_t lc_log_buffer_count(LCLogBuffer *buffer) {
    if (buffer == NULL) {
        return 0;
    }

    if (pthread_mutex_lock(&buffer->mutex) != 0) {
        return 0;
    }
    size_t count = buffer->size;
    pthread_mutex_unlock(&buffer->mutex);
    return count;
}

void lc_log_buffer_get_stats(LCLogBuffer *buffer, LCLogBufferStats *stats) {
    if (buffer == NULL || stats == NULL) {
        return;
    }

    if (pthread_mutex_lock(&buffer->mutex) != 0) {
        memset(stats, 0, sizeof(*stats));
        return;
    }

    stats->current_size = buffer->size;
    stats->total_logs = buffer->total_logs;
    stats->dropped_logs = buffer->dropped_logs;

    pthread_mutex_unlock(&buffer->mutex);
}

int lc_log_buffer_copy_keyword(LCLogBuffer *buffer, const char *keyword,
                               char ***results, size_t *result_count) {
    if (buffer == NULL || keyword == NULL || results == NULL || result_count == NULL) {
        errno = EINVAL;
        return -1;
    }

    *results = NULL;
    *result_count = 0;
    size_t capacity = 0;

    if (pthread_mutex_lock(&buffer->mutex) != 0) {
        return -1;
    }

    size_t start = lc_log_buffer_oldest_index(buffer);
    size_t index = start;

    for (size_t i = 0; i < buffer->size; ++i) {
        LCLogEntry *entry = &buffer->entries[index];
        if (entry->message != NULL && strstr(entry->message, keyword) != NULL) {
            if (lc_log_buffer_append_match(results, result_count, &capacity, entry->message) != 0) {
                pthread_mutex_unlock(&buffer->mutex);
                for (size_t j = 0; j < *result_count; ++j) {
                    free((*results)[j]);
                }
                free(*results);
                *results = NULL;
                *result_count = 0;
                return -1;
            }
        }
        index = (index + 1) % buffer->capacity;
    }

    pthread_mutex_unlock(&buffer->mutex);

    return 0;
}

static int lc_message_contains(const char *message, const char *needle) {
    if (needle == NULL) {
        return 1;
    }
    return strstr(message, needle) != NULL;
}

static int lc_message_keywords_match(const char *message, const LCQueryRequest *request) {
    if (request->keyword_count == 0) {
        return 1;
    }

    if (request->keyword_operator == LC_QUERY_OPERATOR_AND) {
        for (size_t i = 0; i < request->keyword_count; ++i) {
            if (request->keywords[i] == NULL) {
                continue;
            }
            if (strstr(message, request->keywords[i]) == NULL) {
                return 0;
            }
        }
        return 1;
    }

    for (size_t i = 0; i < request->keyword_count; ++i) {
        if (request->keywords[i] != NULL && strstr(message, request->keywords[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int lc_log_buffer_entry_matches(const LCLogEntry *entry, const LCQueryRequest *request) {
    if (entry == NULL || entry->message == NULL) {
        return 0;
    }

    if (!lc_message_contains(entry->message, request->keyword)) {
        return 0;
    }

    if (!lc_message_keywords_match(entry->message, request)) {
        return 0;
    }

    if (request->has_regex) {
        int rc = regexec(&request->regex, entry->message, 0, NULL, 0);
        if (rc != 0) {
            return 0;
        }
    }

    if (request->has_time_from && entry->timestamp < request->time_from) {
        return 0;
    }

    if (request->has_time_to && entry->timestamp > request->time_to) {
        return 0;
    }

    return 1;
}

int lc_log_buffer_execute_query(LCLogBuffer *buffer, const LCQueryRequest *request,
                                char ***results, size_t *result_count) {
    if (buffer == NULL || request == NULL || results == NULL || result_count == NULL) {
        errno = EINVAL;
        return -1;
    }

    *results = NULL;
    *result_count = 0;
    size_t capacity = 0;

    if (pthread_mutex_lock(&buffer->mutex) != 0) {
        return -1;
    }

    size_t start = lc_log_buffer_oldest_index(buffer);
    size_t index = start;

    for (size_t i = 0; i < buffer->size; ++i) {
        LCLogEntry *entry = &buffer->entries[index];
        if (lc_log_buffer_entry_matches(entry, request)) {
            char *formatted = lc_log_buffer_format_entry(entry);
            if (formatted == NULL ||
                lc_log_buffer_append_owned(results, result_count, &capacity, formatted) != 0) {
                if (formatted != NULL) {
                    free(formatted);
                }
                pthread_mutex_unlock(&buffer->mutex);
                for (size_t j = 0; j < *result_count; ++j) {
                    free((*results)[j]);
                }
                free(*results);
                *results = NULL;
                *result_count = 0;
                return -1;
            }
        }
        index = (index + 1) % buffer->capacity;
    }

    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}
