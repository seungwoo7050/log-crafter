/*
 * Sequence: SEQ0023
 * Track: C
 * MVP: mvp4
 * Change: Implement the asynchronous persistence manager with file rotation, retention, and startup replay support.
 * Tests: smoke_persistence_rotation
 */
#include "persistence.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILE_SIZE
#define LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILE_SIZE (10 * 1024 * 1024)
#endif

#ifndef LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILES
#define LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILES 10
#endif

#define LC_PERSISTENCE_LINE_BUFFER 2048

static void *lc_persistence_thread_main(void *arg);
static int lc_persistence_ensure_directory(LCPersistence *persistence);
static int lc_persistence_open_current(LCPersistence *persistence);
static void lc_persistence_close_current(LCPersistence *persistence);
static int lc_persistence_rotate(LCPersistence *persistence, time_t timestamp);
static int lc_persistence_prune(LCPersistence *persistence);
static int lc_persistence_collect_files(const char *directory, int include_current,
                                        char ***names, size_t *count);
static void lc_persistence_free_file_list(char **names, size_t count);
static void lc_persistence_free_entry(LCPersistenceEntry *entry);
static int lc_persistence_write_entry(LCPersistence *persistence, LCPersistenceEntry *entry);
static void lc_persistence_format_timestamp(time_t timestamp, char *buffer, size_t capacity);
static int lc_persistence_safe_localtime(time_t timestamp, struct tm *out);
static time_t lc_persistence_parse_line(const char *line, size_t *offset);
static int lc_persistence_name_compare(const void *lhs, const void *rhs);

int lc_persistence_init(LCPersistence *persistence, const char *directory,
                        size_t max_file_size, size_t max_files) {
    if (persistence == NULL || directory == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(persistence, 0, sizeof(*persistence));

    if (pthread_mutex_init(&persistence->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&persistence->condition, NULL) != 0) {
        pthread_mutex_destroy(&persistence->mutex);
        return -1;
    }

    persistence->max_file_size = (max_file_size == 0) ? LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILE_SIZE
                                                      : max_file_size;
    persistence->max_files = (max_files == 0) ? LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILES : max_files;

    size_t length = strlen(directory);
    if (length >= PATH_MAX) {
        errno = ENAMETOOLONG;
        lc_persistence_shutdown(persistence);
        return -1;
    }
    memcpy(persistence->directory, directory, length + 1);

    if (lc_persistence_ensure_directory(persistence) != 0) {
        lc_persistence_shutdown(persistence);
        return -1;
    }

    if (lc_persistence_open_current(persistence) != 0) {
        lc_persistence_shutdown(persistence);
        return -1;
    }

    if (pthread_create(&persistence->thread, NULL, lc_persistence_thread_main, persistence) != 0) {
        lc_persistence_shutdown(persistence);
        return -1;
    }
    persistence->thread_started = 1;

    return 0;
}

void lc_persistence_shutdown(LCPersistence *persistence) {
    if (persistence == NULL) {
        return;
    }

    pthread_mutex_lock(&persistence->mutex);
    persistence->stop = 1;
    pthread_cond_broadcast(&persistence->condition);
    pthread_mutex_unlock(&persistence->mutex);

    if (persistence->thread_started) {
        pthread_join(persistence->thread, NULL);
        persistence->thread_started = 0;
    }

    lc_persistence_close_current(persistence);

    LCPersistenceEntry *entry = persistence->head;
    while (entry != NULL) {
        LCPersistenceEntry *next = entry->next;
        lc_persistence_free_entry(entry);
        entry = next;
    }
    persistence->head = NULL;
    persistence->tail = NULL;

    pthread_cond_destroy(&persistence->condition);
    pthread_mutex_destroy(&persistence->mutex);
}

int lc_persistence_enqueue(LCPersistence *persistence, const char *message, time_t timestamp) {
    if (persistence == NULL || message == NULL) {
        errno = EINVAL;
        return -1;
    }

    char *copy = strdup(message);
    if (copy == NULL) {
        return -1;
    }

    LCPersistenceEntry *entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        free(copy);
        return -1;
    }

    entry->timestamp = timestamp;
    entry->message = copy;
    entry->next = NULL;

    if (pthread_mutex_lock(&persistence->mutex) != 0) {
        lc_persistence_free_entry(entry);
        return -1;
    }

    if (persistence->stop) {
        pthread_mutex_unlock(&persistence->mutex);
        lc_persistence_free_entry(entry);
        errno = ECANCELED;
        return -1;
    }

    if (persistence->tail == NULL) {
        persistence->head = entry;
        persistence->tail = entry;
    } else {
        persistence->tail->next = entry;
        persistence->tail = entry;
    }
    persistence->queued_logs++;

    pthread_cond_signal(&persistence->condition);
    pthread_mutex_unlock(&persistence->mutex);

    return 0;
}

void lc_persistence_get_stats(LCPersistence *persistence, LCPersistenceStats *stats) {
    if (persistence == NULL || stats == NULL) {
        return;
    }

    if (pthread_mutex_lock(&persistence->mutex) != 0) {
        memset(stats, 0, sizeof(*stats));
        return;
    }

    stats->queued_logs = persistence->queued_logs;
    stats->persisted_logs = persistence->persisted_logs;
    stats->failed_logs = persistence->failed_logs;

    pthread_mutex_unlock(&persistence->mutex);
}

int lc_persistence_load_existing(LCPersistence *persistence,
                                 LCPersistenceReplayCallback callback,
                                 void *user_data) {
    if (persistence == NULL || callback == NULL) {
        errno = EINVAL;
        return -1;
    }

    char **files = NULL;
    size_t file_count = 0;
    if (lc_persistence_collect_files(persistence->directory, 1, &files, &file_count) != 0) {
        return -1;
    }

    int result = 0;

    for (size_t i = 0; i < file_count; ++i) {
        char path[PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%s", persistence->directory, files[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            result = -1;
            break;
        }

        FILE *file = fopen(path, "r");
        if (file == NULL) {
            continue;
        }

        char line[LC_PERSISTENCE_LINE_BUFFER];
        while (fgets(line, sizeof(line), file) != NULL) {
            size_t offset = 0;
            time_t timestamp = lc_persistence_parse_line(line, &offset);

            size_t length = strcspn(line + offset, "\r\n");
            line[offset + length] = '\0';

            callback(line + offset, timestamp, user_data);
        }
        fclose(file);
    }

    lc_persistence_free_file_list(files, file_count);
    return result;
}

static int lc_persistence_safe_localtime(time_t timestamp, struct tm *out) {
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

static void lc_persistence_format_timestamp(time_t timestamp, char *buffer, size_t capacity) {
    struct tm tm_value;
    if (lc_persistence_safe_localtime(timestamp, &tm_value) != 0) {
        memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = 70;
        tm_value.tm_mon = 0;
        tm_value.tm_mday = 1;
    }

    if (strftime(buffer, capacity, "%Y-%m-%d %H:%M:%S", &tm_value) == 0) {
        snprintf(buffer, capacity, "1970-01-01 00:00:00");
    }
}

static void *lc_persistence_thread_main(void *arg) {
    LCPersistence *persistence = (LCPersistence *)arg;

    while (1) {
        if (pthread_mutex_lock(&persistence->mutex) != 0) {
            break;
        }

        while (!persistence->stop && persistence->head == NULL) {
            pthread_cond_wait(&persistence->condition, &persistence->mutex);
        }

        if (persistence->stop && persistence->head == NULL) {
            pthread_mutex_unlock(&persistence->mutex);
            break;
        }

        LCPersistenceEntry *entry = persistence->head;
        if (entry != NULL) {
            persistence->head = entry->next;
            if (persistence->head == NULL) {
                persistence->tail = NULL;
            }
        }
        pthread_mutex_unlock(&persistence->mutex);

        if (entry == NULL) {
            continue;
        }

        if (lc_persistence_write_entry(persistence, entry) != 0) {
            pthread_mutex_lock(&persistence->mutex);
            persistence->failed_logs++;
            pthread_mutex_unlock(&persistence->mutex);
        } else {
            pthread_mutex_lock(&persistence->mutex);
            persistence->persisted_logs++;
            pthread_mutex_unlock(&persistence->mutex);
        }

        lc_persistence_free_entry(entry);
    }

    return NULL;
}

static void lc_persistence_free_entry(LCPersistenceEntry *entry) {
    if (entry == NULL) {
        return;
    }
    free(entry->message);
    free(entry);
}

static int lc_persistence_ensure_directory(LCPersistence *persistence) {
    struct stat st;
    if (stat(persistence->directory, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR;
            return -1;
        }
        return 0;
    }

    if (mkdir(persistence->directory, 0775) != 0) {
        return -1;
    }
    return 0;
}

static int lc_persistence_open_current(LCPersistence *persistence) {
    int written = snprintf(persistence->current_path, sizeof(persistence->current_path),
                           "%s/current.log", persistence->directory);
    if (written <= 0 || (size_t)written >= sizeof(persistence->current_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    persistence->current_file = fopen(persistence->current_path, "a+");
    if (persistence->current_file == NULL) {
        return -1;
    }

    if (fseek(persistence->current_file, 0, SEEK_END) == 0) {
        long pos = ftell(persistence->current_file);
        if (pos >= 0) {
            persistence->current_size = (size_t)pos;
        }
    }

    return 0;
}

static void lc_persistence_close_current(LCPersistence *persistence) {
    if (persistence->current_file != NULL) {
        fflush(persistence->current_file);
        fclose(persistence->current_file);
        persistence->current_file = NULL;
    }
    persistence->current_size = 0;
}

static int lc_persistence_rotate(LCPersistence *persistence, time_t timestamp) {
    lc_persistence_close_current(persistence);

    char timestamp_buffer[32];
    lc_persistence_format_timestamp(timestamp, timestamp_buffer, sizeof(timestamp_buffer));

    char rotated_name[64];
    if (snprintf(rotated_name, sizeof(rotated_name), "%s.log", timestamp_buffer) < 0) {
        return -1;
    }

    char rotated_path[PATH_MAX];
    int written = snprintf(rotated_path, sizeof(rotated_path), "%s/%s", persistence->directory, rotated_name);
    if (written <= 0 || (size_t)written >= sizeof(rotated_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (rename(persistence->current_path, rotated_path) != 0) {
        if (errno != ENOENT) {
            return -1;
        }
    }

    if (lc_persistence_open_current(persistence) != 0) {
        return -1;
    }

    if (lc_persistence_prune(persistence) != 0) {
        /* Non-fatal: continue even if pruning fails */
    }

    return 0;
}

static int lc_persistence_prune(LCPersistence *persistence) {
    if (persistence->max_files == 0) {
        return 0;
    }

    char **files = NULL;
    size_t count = 0;
    if (lc_persistence_collect_files(persistence->directory, 0, &files, &count) != 0) {
        return -1;
    }

    int result = 0;
    if (count > persistence->max_files) {
        size_t excess = count - persistence->max_files;
        for (size_t i = 0; i < excess; ++i) {
            char path[PATH_MAX];
            int written = snprintf(path, sizeof(path), "%s/%s", persistence->directory, files[i]);
            if (written <= 0 || (size_t)written >= sizeof(path)) {
                result = -1;
                break;
            }
            unlink(path);
        }
    }

    lc_persistence_free_file_list(files, count);
    return result;
}

static int lc_persistence_name_compare(const void *lhs, const void *rhs) {
    const char *const *left = (const char *const *)lhs;
    const char *const *right = (const char *const *)rhs;
    return strcmp(*left, *right);
}

static int lc_persistence_collect_files(const char *directory, int include_current,
                                        char ***names, size_t *count) {
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        return -1;
    }

    struct dirent *entry;
    size_t capacity = 0;
    *names = NULL;
    *count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (!include_current && strcmp(entry->d_name, "current.log") == 0) {
            continue;
        }

        size_t length = strlen(entry->d_name);
        if (length < 4 || strcmp(entry->d_name + length - 4, ".log") != 0) {
            continue;
        }

        if (capacity == *count) {
            size_t new_capacity = (capacity == 0) ? 8 : capacity * 2;
            char **resized = realloc(*names, new_capacity * sizeof(char *));
            if (resized == NULL) {
                lc_persistence_free_file_list(*names, *count);
                closedir(dir);
                return -1;
            }
            *names = resized;
            capacity = new_capacity;
        }

        (*names)[*count] = strdup(entry->d_name);
        if ((*names)[*count] == NULL) {
            lc_persistence_free_file_list(*names, *count);
            closedir(dir);
            return -1;
        }
        (*count)++;
    }

    closedir(dir);

    if (*count > 1) {
        qsort(*names, *count, sizeof(char *), lc_persistence_name_compare);
    }

    if (include_current) {
        for (size_t i = 0; i + 1 < *count; ++i) {
            if (strcmp((*names)[i], "current.log") == 0) {
                char *current = (*names)[i];
                for (size_t j = i; j + 1 < *count; ++j) {
                    (*names)[j] = (*names)[j + 1];
                }
                (*names)[*count - 1] = current;
                break;
            }
        }
    }

    return 0;
}

static void lc_persistence_free_file_list(char **names, size_t count) {
    if (names == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        free(names[i]);
    }
    free(names);
}

static int lc_persistence_write_entry(LCPersistence *persistence, LCPersistenceEntry *entry) {
    if (entry == NULL) {
        return -1;
    }

    if (persistence->current_file == NULL) {
        if (lc_persistence_open_current(persistence) != 0) {
            return -1;
        }
    }

    char timestamp[32];
    lc_persistence_format_timestamp(entry->timestamp, timestamp, sizeof(timestamp));

    size_t message_length = strlen(entry->message);
    size_t total_length = strlen(timestamp) + message_length + 5; /* [] + space + \n */
    char *line = malloc(total_length);
    if (line == NULL) {
        return -1;
    }

    int written = snprintf(line, total_length, "[%s] %s\n", timestamp, entry->message);
    if (written < 0) {
        free(line);
        return -1;
    }

    size_t bytes = (size_t)written;
    size_t written_bytes = fwrite(line, 1, bytes, persistence->current_file);
    free(line);
    if (written_bytes != bytes) {
        return -1;
    }

    persistence->current_size += bytes;
    fflush(persistence->current_file);

    if (persistence->max_file_size > 0 && persistence->current_size >= persistence->max_file_size) {
        if (lc_persistence_rotate(persistence, entry->timestamp) != 0) {
            return -1;
        }
    }

    return 0;
}

static time_t lc_persistence_parse_line(const char *line, size_t *offset) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int consumed = 0;
    if (sscanf(line, "[%d-%d-%d %d:%d:%d] %n", &year, &month, &day, &hour, &minute, &second, &consumed) == 6) {
        struct tm tm_value;
        memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = year - 1900;
        tm_value.tm_mon = month - 1;
        tm_value.tm_mday = day;
        tm_value.tm_hour = hour;
        tm_value.tm_min = minute;
        tm_value.tm_sec = second;
        tm_value.tm_isdst = -1;
        time_t ts = mktime(&tm_value);
        if (offset != NULL) {
            *offset = (size_t)consumed;
        }
        return ts;
    }
    if (offset != NULL) {
        *offset = 0;
    }
    return (time_t)0;
}
