#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define DEFAULT_BUFFER_SIZE 10000
#define MAX_LOG_LENGTH 4096

// [SEQUENCE: 88] Log entry structure
typedef struct log_entry {
    char* message;
    time_t timestamp;
    struct log_entry* next;
} log_entry_t;

// [SEQUENCE: 89] Thread-safe circular log buffer
typedef struct {
    log_entry_t** entries;    // Circular array of log entries
    size_t capacity;          // Maximum number of entries
    size_t size;              // Current number of entries
    size_t head;              // Index for next write
    size_t tail;              // Index for next read
    
    // [SEQUENCE: 90] Synchronization for thread safety
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    
    // [SEQUENCE: 91] Statistics tracking
    unsigned long total_logs;
    unsigned long dropped_logs;
} log_buffer_t;

// [SEQUENCE: 92] Buffer lifecycle functions
log_buffer_t* log_buffer_create(size_t capacity);
void log_buffer_destroy(log_buffer_t* buffer);

// [SEQUENCE: 93] Buffer operations
int log_buffer_push(log_buffer_t* buffer, const char* message);
char* log_buffer_pop(log_buffer_t* buffer);
int log_buffer_search(log_buffer_t* buffer, const char* keyword, char*** results, int* count);

// [SEQUENCE: 259] Enhanced search with query parser
// Forward declaration from query_parser.h
struct parsed_query;
int log_buffer_search_enhanced(log_buffer_t* buffer, const struct parsed_query* query, 
                              char*** results, int* count);

// [SEQUENCE: 94] Buffer status functions
size_t log_buffer_size(log_buffer_t* buffer);
bool log_buffer_is_empty(log_buffer_t* buffer);
bool log_buffer_is_full(log_buffer_t* buffer);
void log_buffer_get_stats(log_buffer_t* buffer, unsigned long* total, unsigned long* dropped);

#endif // LOG_BUFFER_H
