#include "log_buffer.h"
#include "query_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// [SEQUENCE: 95] Create thread-safe log buffer
log_buffer_t* log_buffer_create(size_t capacity) {
    if (capacity == 0) {
        capacity = DEFAULT_BUFFER_SIZE;
    }
    
    log_buffer_t* buffer = malloc(sizeof(log_buffer_t));
    if (!buffer) {
        perror("malloc");
        return NULL;
    }
    
    // [SEQUENCE: 96] Allocate circular array
    buffer->entries = calloc(capacity, sizeof(log_entry_t*));
    if (!buffer->entries) {
        free(buffer);
        perror("calloc");
        return NULL;
    }
    
    // [SEQUENCE: 97] Initialize buffer state
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->total_logs = 0;
    buffer->dropped_logs = 0;
    
    // [SEQUENCE: 98] Initialize synchronization
    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->entries);
        free(buffer);
        return NULL;
    }
    
    if (pthread_cond_init(&buffer->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->entries);
        free(buffer);
        return NULL;
    }
    
    if (pthread_cond_init(&buffer->not_full, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        pthread_cond_destroy(&buffer->not_empty);
        free(buffer->entries);
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

// [SEQUENCE: 99] Push log entry to buffer
int log_buffer_push(log_buffer_t* buffer, const char* message) {
    if (!buffer || !message) {
        return -1;
    }
    
    // [SEQUENCE: 100] Create new log entry
    log_entry_t* entry = malloc(sizeof(log_entry_t));
    if (!entry) {
        perror("malloc");
        return -1;
    }
    
    entry->message = strdup(message);
    if (!entry->message) {
        free(entry);
        perror("strdup");
        return -1;
    }
    entry->timestamp = time(NULL);
    entry->next = NULL;
    
    // [SEQUENCE: 101] Add entry to buffer with locking
    pthread_mutex_lock(&buffer->mutex);
    
    // Check if buffer is full
    if (buffer->size == buffer->capacity) {
        // [SEQUENCE: 102] Drop oldest entry when full
        log_entry_t* old_entry = buffer->entries[buffer->tail];
        if (old_entry) {
            free(old_entry->message);
            free(old_entry);
        }
        buffer->tail = (buffer->tail + 1) % buffer->capacity;
        buffer->size--;
        buffer->dropped_logs++;
    }
    
    // [SEQUENCE: 103] Insert new entry at head
    buffer->entries[buffer->head] = entry;
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    buffer->total_logs++;
    
    // [SEQUENCE: 104] Signal waiting consumers
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
    
    return 0;
}

// [SEQUENCE: 105] Pop log entry from buffer
char* log_buffer_pop(log_buffer_t* buffer) {
    if (!buffer) {
        return NULL;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    // [SEQUENCE: 106] Wait if buffer is empty
    while (buffer->size == 0) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }
    
    // [SEQUENCE: 107] Remove entry from tail
    log_entry_t* entry = buffer->entries[buffer->tail];
    buffer->entries[buffer->tail] = NULL;
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
    
    // [SEQUENCE: 108] Signal waiting producers
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    
    if (entry) {
        char* message = entry->message;
        free(entry);
        return message;
    }
    
    return NULL;
}

// [SEQUENCE: 109] Search for logs containing keyword
int log_buffer_search(log_buffer_t* buffer, const char* keyword, char*** results, int* count) {
    if (!buffer || !keyword || !results || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    // [SEQUENCE: 110] Count matching entries
    *count = 0;
    size_t idx = buffer->tail;
    for (size_t i = 0; i < buffer->size; i++) {
        if (buffer->entries[idx] && buffer->entries[idx]->message) {
            if (strstr(buffer->entries[idx]->message, keyword)) {
                (*count)++;
            }
        }
        idx = (idx + 1) % buffer->capacity;
    }
    
    if (*count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        *results = NULL;
        return 0;
    }
    
    // [SEQUENCE: 111] Allocate results array
    *results = malloc(sizeof(char*) * (*count));
    if (!*results) {
        pthread_mutex_unlock(&buffer->mutex);
        *count = 0;
        return -1;
    }
    
    // [SEQUENCE: 112] Copy matching entries
    int result_idx = 0;
    idx = buffer->tail;
    for (size_t i = 0; i < buffer->size && result_idx < *count; i++) {
        if (buffer->entries[idx] && buffer->entries[idx]->message) {
            if (strstr(buffer->entries[idx]->message, keyword)) {
                (*results)[result_idx] = strdup(buffer->entries[idx]->message);
                if (!(*results)[result_idx]) {
                    // Cleanup on failure
                    for (int j = 0; j < result_idx; j++) {
                        free((*results)[j]);
                    }
                    free(*results);
                    *results = NULL;
                    *count = 0;
                    pthread_mutex_unlock(&buffer->mutex);
                    return -1;
                }
                result_idx++;
            }
        }
        idx = (idx + 1) % buffer->capacity;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

// [SEQUENCE: 113] Get buffer size
size_t log_buffer_size(log_buffer_t* buffer) {
    if (!buffer) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    size_t size = buffer->size;
    pthread_mutex_unlock(&buffer->mutex);
    
    return size;
}

// [SEQUENCE: 114] Check if buffer is empty
bool log_buffer_is_empty(log_buffer_t* buffer) {
    return log_buffer_size(buffer) == 0;
}

// [SEQUENCE: 115] Check if buffer is full
bool log_buffer_is_full(log_buffer_t* buffer) {
    if (!buffer) return false;
    
    pthread_mutex_lock(&buffer->mutex);
    bool full = (buffer->size == buffer->capacity);
    pthread_mutex_unlock(&buffer->mutex);
    
    return full;
}

// [SEQUENCE: 116] Get buffer statistics
void log_buffer_get_stats(log_buffer_t* buffer, unsigned long* total, unsigned long* dropped) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer->mutex);
    if (total) *total = buffer->total_logs;
    if (dropped) *dropped = buffer->dropped_logs;
    pthread_mutex_unlock(&buffer->mutex);
}

// [SEQUENCE: 117] Destroy log buffer and free resources
void log_buffer_destroy(log_buffer_t* buffer) {
    if (!buffer) return;
    
    // [SEQUENCE: 118] Free all log entries
    for (size_t i = 0; i < buffer->capacity; i++) {
        if (buffer->entries[i]) {
            free(buffer->entries[i]->message);
            free(buffer->entries[i]);
        }
    }
    
    // [SEQUENCE: 119] Cleanup synchronization and memory
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
    
    free(buffer->entries);
    free(buffer);
}

// [SEQUENCE: 260] Enhanced search with query parser
int log_buffer_search_enhanced(log_buffer_t* buffer, const struct parsed_query* query, 
                              char*** results, int* count) {
    if (!buffer || !query || !results || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    // [SEQUENCE: 261] Count matching entries using query parser
    *count = 0;
    size_t idx = buffer->tail;
    for (size_t i = 0; i < buffer->size; i++) {
        if (buffer->entries[idx] && buffer->entries[idx]->message) {
            if (query_matches_log((const parsed_query_t*)query, buffer->entries[idx]->message, 
                                buffer->entries[idx]->timestamp)) {
                (*count)++;
            }
        }
        idx = (idx + 1) % buffer->capacity;
    }
    
    if (*count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        *results = NULL;
        return 0;
    }
    
    // [SEQUENCE: 262] Allocate results array
    *results = malloc(sizeof(char*) * (*count));
    if (!*results) {
        pthread_mutex_unlock(&buffer->mutex);
        *count = 0;
        return -1;
    }
    
    // [SEQUENCE: 263] Copy matching entries with timestamps
    int result_idx = 0;
    idx = buffer->tail;
    for (size_t i = 0; i < buffer->size && result_idx < *count; i++) {
        if (buffer->entries[idx] && buffer->entries[idx]->message) {
            if (query_matches_log((const parsed_query_t*)query, buffer->entries[idx]->message,
                                buffer->entries[idx]->timestamp)) {
                // Format result with timestamp
                char timestamp_str[32];
                struct tm* tm_info = localtime(&buffer->entries[idx]->timestamp);
                strftime(timestamp_str, sizeof(timestamp_str), 
                        "%Y-%m-%d %H:%M:%S", tm_info);
                
                // Allocate space for formatted result
                size_t result_len = strlen(timestamp_str) + strlen(buffer->entries[idx]->message) + 4;
                (*results)[result_idx] = malloc(result_len);
                
                if (!(*results)[result_idx]) {
                    // Cleanup on failure
                    for (int j = 0; j < result_idx; j++) {
                        free((*results)[j]);
                    }
                    free(*results);
                    *results = NULL;
                    *count = 0;
                    pthread_mutex_unlock(&buffer->mutex);
                    return -1;
                }
                
                snprintf((*results)[result_idx], result_len, "[%s] %s", 
                        timestamp_str, buffer->entries[idx]->message);
                result_idx++;
            }
        }
        idx = (idx + 1) % buffer->capacity;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}
