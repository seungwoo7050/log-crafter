/*
 * Sequence: SEQ0007
 * Track: C
 * MVP: mvp2
 * Change: Define the pthread-backed job queue API for the MVP2 server workers.
 * Tests: smoke_thread_pool_query
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stddef.h>

typedef void (*LCThreadPoolJobFn)(void *arg);

typedef struct LCThreadPoolJob {
    LCThreadPoolJobFn fn;
    void *arg;
    struct LCThreadPoolJob *next;
} LCThreadPoolJob;

typedef struct LCThreadPool {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t *threads;
    size_t thread_count;
    LCThreadPoolJob *head;
    LCThreadPoolJob *tail;
    int stop;
} LCThreadPool;

int thread_pool_init(LCThreadPool *pool, size_t thread_count);
int thread_pool_submit(LCThreadPool *pool, LCThreadPoolJobFn fn, void *arg);
void thread_pool_shutdown(LCThreadPool *pool);

#endif /* THREAD_POOL_H */
