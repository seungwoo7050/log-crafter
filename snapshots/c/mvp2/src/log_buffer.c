/*
 * Sequence: SEQ0006
 * Track: C
 * MVP: mvp2
 * Change: Provide the circular buffer implementation with keyword search copies for query workers.
 * Tests: smoke_thread_pool_query
 */
#include "log_buffer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

int lc_log_buffer_push(LCLogBuffer *buffer, const char *message) {
    if (buffer == NULL || message == NULL) {
        errno = EINVAL;
        return -1;
    }

    char *copy = strdup(message);
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
    slot->timestamp = time(NULL);

    buffer->head = (buffer->head + 1) % buffer->capacity;
    if (buffer->size < buffer->capacity) {
        buffer->size++;
    }
    buffer->total_logs++;

    pthread_mutex_unlock(&buffer->mutex);
    return 0;
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

static int lc_log_buffer_append_match(char ***results, size_t *count, size_t *capacity, const char *message) {
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
