#include "persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// [SEQUENCE: 311] Internal persistence manager structure
struct persistence_manager {
    persistence_config_t config;
    FILE* current_file;
    char current_filename[512];
    size_t current_size;
    pthread_mutex_t mutex;
    pthread_t writer_thread;
    bool running;
    
    // Async write queue
    struct write_entry {
        char* log_entry;
        time_t timestamp;
        struct write_entry* next;
    } *write_queue_head, *write_queue_tail;
    
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    
    // Statistics
    persistence_stats_t stats;
};

// [SEQUENCE: 312] Create log directory if it doesn't exist
static int ensure_log_directory(const char* path) {
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    
    return 0;
}

// [SEQUENCE: 313] Generate new log filename with timestamp
static void generate_filename(char* buffer, size_t size, const char* directory, bool current) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    if (current) {
        snprintf(buffer, size, "%s/current.log", directory);
    } else {
        // [SEQUENCE: 314] Use timestamp for rotated files
        snprintf(buffer, size, "%s/%04d-%02d-%02d-%03d.log",
                directory,
                tm_info->tm_year + 1900,
                tm_info->tm_mon + 1,
                tm_info->tm_mday,
                (int)(now % 1000));
    }
}

// [SEQUENCE: 315] Open or create current log file
static FILE* open_log_file(persistence_manager_t* manager) {
    generate_filename(manager->current_filename, sizeof(manager->current_filename),
                     manager->config.log_directory, true);
    
    FILE* file = fopen(manager->current_filename, "a");
    if (!file) {
        perror("fopen");
        return NULL;
    }
    
    // [SEQUENCE: 316] Get current file size
    fseek(file, 0, SEEK_END);
    manager->current_size = ftell(file);
    
    return file;
}

// [SEQUENCE: 317] Write log entry to file
static int write_to_file(persistence_manager_t* manager, const char* log_entry, time_t timestamp) {
    if (!manager->current_file) {
        manager->current_file = open_log_file(manager);
        if (!manager->current_file) {
            manager->stats.write_errors++;
            return -1;
        }
    }
    
    // [SEQUENCE: 318] Format: [timestamp] log_entry\n
    struct tm* tm_info = localtime(&timestamp);
    int written = fprintf(manager->current_file, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                         tm_info->tm_year + 1900,
                         tm_info->tm_mon + 1,
                         tm_info->tm_mday,
                         tm_info->tm_hour,
                         tm_info->tm_min,
                         tm_info->tm_sec,
                         log_entry);
    
    if (written < 0) {
        manager->stats.write_errors++;
        return -1;
    }
    
    manager->current_size += written;
    manager->stats.total_written++;
    
    // [SEQUENCE: 319] Check if rotation needed
    if (manager->current_size >= manager->config.max_file_size) {
        persistence_rotate(manager);
    }
    
    return 0;
}

// [SEQUENCE: 320] Async writer thread function
static void* writer_thread_func(void* arg) {
    persistence_manager_t* manager = (persistence_manager_t*)arg;
    
    while (manager->running) {
        pthread_mutex_lock(&manager->queue_mutex);
        
        // [SEQUENCE: 321] Wait for entries or timeout
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += manager->config.flush_interval_ms * 1000000;
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }
        
        pthread_cond_timedwait(&manager->queue_cond, &manager->queue_mutex, &timeout);
        
        // [SEQUENCE: 322] Process all queued entries
        struct write_entry* entry = manager->write_queue_head;
        manager->write_queue_head = NULL;
        manager->write_queue_tail = NULL;
        
        pthread_mutex_unlock(&manager->queue_mutex);
        
        // [SEQUENCE: 323] Write entries to file
        pthread_mutex_lock(&manager->mutex);
        while (entry) {
            write_to_file(manager, entry->log_entry, entry->timestamp);
            struct write_entry* next = entry->next;
            free(entry->log_entry);
            free(entry);
            entry = next;
        }
        
        // [SEQUENCE: 324] Flush to disk
        if (manager->current_file) {
            fflush(manager->current_file);
        }
        
        pthread_mutex_unlock(&manager->mutex);
    }
    
    return NULL;
}

// [SEQUENCE: 325] Create persistence manager implementation
persistence_manager_t* persistence_create(const persistence_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    persistence_manager_t* manager = calloc(1, sizeof(persistence_manager_t));
    if (!manager) {
        return NULL;
    }
    
    // [SEQUENCE: 326] Copy configuration
    manager->config = *config;
    
    // [SEQUENCE: 327] Initialize mutexes
    if (pthread_mutex_init(&manager->mutex, NULL) != 0 ||
        pthread_mutex_init(&manager->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&manager->queue_cond, NULL) != 0) {
        free(manager);
        return NULL;
    }
    
    // [SEQUENCE: 328] Create log directory
    if (manager->config.enabled) {
        if (ensure_log_directory(manager->config.log_directory) != 0) {
            pthread_mutex_destroy(&manager->mutex);
            pthread_mutex_destroy(&manager->queue_mutex);
            pthread_cond_destroy(&manager->queue_cond);
            free(manager);
            return NULL;
        }
        
        // [SEQUENCE: 329] Start writer thread
        manager->running = true;
        if (pthread_create(&manager->writer_thread, NULL, writer_thread_func, manager) != 0) {
            pthread_mutex_destroy(&manager->mutex);
            pthread_mutex_destroy(&manager->queue_mutex);
            pthread_cond_destroy(&manager->queue_cond);
            free(manager);
            return NULL;
        }
    }
    
    return manager;
}

// [SEQUENCE: 330] Destroy persistence manager
void persistence_destroy(persistence_manager_t* manager) {
    if (!manager) {
        return;
    }
    
    // [SEQUENCE: 331] Stop writer thread
    if (manager->config.enabled) {
        manager->running = false;
        pthread_cond_signal(&manager->queue_cond);
        pthread_join(manager->writer_thread, NULL);
    }
    
    // [SEQUENCE: 332] Flush and close file
    pthread_mutex_lock(&manager->mutex);
    if (manager->current_file) {
        fflush(manager->current_file);
        fclose(manager->current_file);
    }
    pthread_mutex_unlock(&manager->mutex);
    
    // [SEQUENCE: 333] Clean up queue
    struct write_entry* entry = manager->write_queue_head;
    while (entry) {
        struct write_entry* next = entry->next;
        free(entry->log_entry);
        free(entry);
        entry = next;
    }
    
    // [SEQUENCE: 334] Destroy mutexes
    pthread_mutex_destroy(&manager->mutex);
    pthread_mutex_destroy(&manager->queue_mutex);
    pthread_cond_destroy(&manager->queue_cond);
    
    free(manager);
}

// [SEQUENCE: 335] Queue log entry for async write
int persistence_write(persistence_manager_t* manager, const char* log_entry, time_t timestamp) {
    if (!manager || !manager->config.enabled || !log_entry) {
        return -1;
    }
    
    // [SEQUENCE: 336] Create write entry
    struct write_entry* entry = malloc(sizeof(struct write_entry));
    if (!entry) {
        return -1;
    }
    
    entry->log_entry = strdup(log_entry);
    if (!entry->log_entry) {
        free(entry);
        return -1;
    }
    
    entry->timestamp = timestamp;
    entry->next = NULL;
    
    // [SEQUENCE: 337] Add to queue
    pthread_mutex_lock(&manager->queue_mutex);
    
    if (manager->write_queue_tail) {
        manager->write_queue_tail->next = entry;
    } else {
        manager->write_queue_head = entry;
    }
    manager->write_queue_tail = entry;
    
    pthread_cond_signal(&manager->queue_cond);
    pthread_mutex_unlock(&manager->queue_mutex);
    
    return 0;
}

// [SEQUENCE: 338] Force flush pending writes
int persistence_flush(persistence_manager_t* manager) {
    if (!manager || !manager->config.enabled) {
        return -1;
    }
    
    // Signal writer thread to flush immediately
    pthread_cond_signal(&manager->queue_cond);
    
    // Wait a bit for flush to complete
    usleep(10000); // 10ms
    
    return 0;
}

// [SEQUENCE: 339] Rotate log file
int persistence_rotate(persistence_manager_t* manager) {
    if (!manager || !manager->current_file) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    // [SEQUENCE: 340] Close current file
    fflush(manager->current_file);
    fclose(manager->current_file);
    manager->current_file = NULL;
    
    // [SEQUENCE: 341] Rename current.log to timestamped file
    char new_filename[512];
    generate_filename(new_filename, sizeof(new_filename), 
                     manager->config.log_directory, false);
    
    if (rename(manager->current_filename, new_filename) != 0) {
        perror("rename");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    manager->stats.files_rotated++;
    manager->current_size = 0;
    
    // [SEQUENCE: 342] Clean up old files if needed
    if (manager->config.max_files > 0) {
        // TODO: Implement file cleanup based on max_files
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

// [SEQUENCE: 343] Load logs from disk
int persistence_load(persistence_manager_t* manager,
                    void (*callback)(const char* log_entry, time_t timestamp, void* user_data),
                    void* user_data) {
    if (!manager || !manager->config.load_on_startup || !callback) {
        return 0;
    }
    
    DIR* dir = opendir(manager->config.log_directory);
    if (!dir) {
        return -1;
    }
    
    struct dirent* entry;
    int loaded_count = 0;
    
    // [SEQUENCE: 344] Read all log files
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".log") == NULL) {
            continue;
        }
        
        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s/%s", 
                manager->config.log_directory, entry->d_name);
        
        FILE* file = fopen(filepath, "r");
        if (!file) {
            continue;
        }
        
        // [SEQUENCE: 345] Parse each line
        char line[4096];
        while (fgets(line, sizeof(line), file)) {
            // Remove newline
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
            }
            
            // [SEQUENCE: 346] Parse timestamp and log entry
            // Format: [YYYY-MM-DD HH:MM:SS] log_entry
            if (line[0] == '[' && len > 21) {
                struct tm tm_info = {0};
                if (sscanf(line, "[%d-%d-%d %d:%d:%d]",
                          &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday,
                          &tm_info.tm_hour, &tm_info.tm_min, &tm_info.tm_sec) == 6) {
                    tm_info.tm_year -= 1900;
                    tm_info.tm_mon -= 1;
                    time_t timestamp = mktime(&tm_info);
                    
                    // Find start of log entry
                    char* log_start = strchr(line + 20, ']');
                    if (log_start && *(log_start + 1) == ' ') {
                        callback(log_start + 2, timestamp, user_data);
                        loaded_count++;
                    }
                }
            }
        }
        
        fclose(file);
    }
    
    closedir(dir);
    
    return loaded_count;
}

// [SEQUENCE: 347] Get persistence statistics
persistence_stats_t persistence_get_stats(const persistence_manager_t* manager) {
    if (!manager) {
        persistence_stats_t empty = {0};
        return empty;
    }
    
    persistence_stats_t stats;
    pthread_mutex_lock((pthread_mutex_t*)&manager->mutex);
    stats = manager->stats;
    stats.current_file_size = manager->current_size;
    pthread_mutex_unlock((pthread_mutex_t*)&manager->mutex);
    
    return stats;
}
