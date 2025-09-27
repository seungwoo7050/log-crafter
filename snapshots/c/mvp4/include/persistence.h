/*
 * Sequence: SEQ0018
 * Track: C
 * MVP: mvp4
 * Change: Introduce the asynchronous persistence manager interface with rotation and replay hooks.
 * Tests: smoke_persistence_rotation
 */
#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LCPersistenceEntry {
    time_t timestamp;
    char *message;
    struct LCPersistenceEntry *next;
} LCPersistenceEntry;

typedef struct LCPersistenceStats {
    unsigned long queued_logs;
    unsigned long persisted_logs;
    unsigned long failed_logs;
} LCPersistenceStats;

typedef void (*LCPersistenceReplayCallback)(const char *message, time_t timestamp, void *user_data);

typedef struct LCPersistence {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int thread_started;
    int stop;
    char directory[PATH_MAX];
    char current_path[PATH_MAX];
    FILE *current_file;
    size_t current_size;
    size_t max_file_size;
    size_t max_files;
    LCPersistenceEntry *head;
    LCPersistenceEntry *tail;
    unsigned long queued_logs;
    unsigned long persisted_logs;
    unsigned long failed_logs;
} LCPersistence;

int lc_persistence_init(LCPersistence *persistence, const char *directory,
                        size_t max_file_size, size_t max_files);
void lc_persistence_shutdown(LCPersistence *persistence);
int lc_persistence_enqueue(LCPersistence *persistence, const char *message, time_t timestamp);
void lc_persistence_get_stats(LCPersistence *persistence, LCPersistenceStats *stats);
int lc_persistence_load_existing(LCPersistence *persistence,
                                 LCPersistenceReplayCallback callback,
                                 void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* PERSISTENCE_H */
