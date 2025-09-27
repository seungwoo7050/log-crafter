/*
 * Sequence: SEQ0020
 * Track: C
 * MVP: mvp4
 * Change: Add timestamp-aware push helpers so persistence replay can hydrate the circular buffer deterministically.
 * Tests: smoke_persistence_rotation
 */
#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <pthread.h>
#include <stddef.h>
#include <time.h>

#include "query_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCLogEntry {
    time_t timestamp;
    char *message;
} LCLogEntry;

typedef struct LCLogBufferStats {
    size_t current_size;
    unsigned long total_logs;
    unsigned long dropped_logs;
} LCLogBufferStats;

typedef struct LCLogBuffer {
    pthread_mutex_t mutex;
    size_t capacity;
    size_t size;
    size_t head;
    unsigned long total_logs;
    unsigned long dropped_logs;
    LCLogEntry *entries;
} LCLogBuffer;

int lc_log_buffer_init(LCLogBuffer *buffer, size_t capacity);
void lc_log_buffer_destroy(LCLogBuffer *buffer);
int lc_log_buffer_push_with_time(LCLogBuffer *buffer, const char *message, time_t timestamp);
int lc_log_buffer_push(LCLogBuffer *buffer, const char *message);
size_t lc_log_buffer_count(LCLogBuffer *buffer);
void lc_log_buffer_get_stats(LCLogBuffer *buffer, LCLogBufferStats *stats);
int lc_log_buffer_copy_keyword(LCLogBuffer *buffer, const char *keyword,
                               char ***results, size_t *result_count);
int lc_log_buffer_execute_query(LCLogBuffer *buffer, const LCQueryRequest *request,
                                char ***results, size_t *result_count);

#ifdef __cplusplus
}
#endif

#endif /* LOG_BUFFER_H */
