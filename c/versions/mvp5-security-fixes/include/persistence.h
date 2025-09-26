#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// [SEQUENCE: 302] Persistence configuration structure
typedef struct {
    bool enabled;                    // Enable/disable persistence
    char log_directory[256];         // Directory for log files
    size_t max_file_size;           // Max size before rotation (bytes)
    int max_files;                  // Max number of rotated files
    int flush_interval_ms;          // Flush interval in milliseconds
    bool load_on_startup;           // Load existing logs on startup
} persistence_config_t;

// [SEQUENCE: 303] Persistence manager structure
typedef struct persistence_manager persistence_manager_t;

// [SEQUENCE: 304] Create persistence manager
persistence_manager_t* persistence_create(const persistence_config_t* config);

// [SEQUENCE: 305] Destroy persistence manager
void persistence_destroy(persistence_manager_t* manager);

// [SEQUENCE: 306] Write log entry to disk
int persistence_write(persistence_manager_t* manager, const char* log_entry, time_t timestamp);

// [SEQUENCE: 307] Flush pending writes
int persistence_flush(persistence_manager_t* manager);

// [SEQUENCE: 308] Rotate log file if needed
int persistence_rotate(persistence_manager_t* manager);

// [SEQUENCE: 309] Load logs from disk on startup
int persistence_load(persistence_manager_t* manager, 
                    void (*callback)(const char* log_entry, time_t timestamp, void* user_data),
                    void* user_data);

// [SEQUENCE: 310] Get persistence statistics
typedef struct {
    size_t total_written;
    size_t current_file_size;
    int files_rotated;
    int write_errors;
} persistence_stats_t;

persistence_stats_t persistence_get_stats(const persistence_manager_t* manager);

#endif // PERSISTENCE_H
