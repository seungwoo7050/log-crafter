/*
 * Sequence: SEQ0008
 * Track: C
 * MVP: mvp2
 * Change: Implement the pthread worker pool with a blocking job queue for concurrent client handling.
 * Tests: smoke_thread_pool_query
 */
#include "thread_pool.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void *thread_pool_worker(void *arg) {
    LCThreadPool *pool = (LCThreadPool *)arg;

    for (;;) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->stop && pool->head == NULL) {
            pthread_cond_wait(&pool->condition, &pool->mutex);
        }

        if (pool->stop && pool->head == NULL) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        LCThreadPoolJob *job = pool->head;
        if (job != NULL) {
            pool->head = job->next;
            if (pool->head == NULL) {
                pool->tail = NULL;
            }
        }
        pthread_mutex_unlock(&pool->mutex);

        if (job != NULL) {
            job->fn(job->arg);
            free(job);
        }
    }

    return NULL;
}

int thread_pool_init(LCThreadPool *pool, size_t thread_count) {
    if (pool == NULL || thread_count == 0) {
        errno = EINVAL;
        return -1;
    }

    memset(pool, 0, sizeof(*pool));
    pool->thread_count = thread_count;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&pool->condition, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        return -1;
    }

    pool->threads = calloc(thread_count, sizeof(pthread_t));
    if (pool->threads == NULL) {
        pthread_cond_destroy(&pool->condition);
        pthread_mutex_destroy(&pool->mutex);
        return -1;
    }

    for (size_t i = 0; i < thread_count; ++i) {
        if (pthread_create(&pool->threads[i], NULL, thread_pool_worker, pool) != 0) {
            pool->stop = 1;
            pthread_cond_broadcast(&pool->condition);
            for (size_t j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pool->threads = NULL;
            pthread_cond_destroy(&pool->condition);
            pthread_mutex_destroy(&pool->mutex);
            return -1;
        }
    }

    return 0;
}

int thread_pool_submit(LCThreadPool *pool, LCThreadPoolJobFn fn, void *arg) {
    if (pool == NULL || fn == NULL) {
        errno = EINVAL;
        return -1;
    }

    LCThreadPoolJob *job = (LCThreadPoolJob *)malloc(sizeof(LCThreadPoolJob));
    if (job == NULL) {
        return -1;
    }
    job->fn = fn;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&pool->mutex);
    if (pool->stop) {
        pthread_mutex_unlock(&pool->mutex);
        free(job);
        errno = EINVAL;
        return -1;
    }

    if (pool->tail == NULL) {
        pool->head = job;
        pool->tail = job;
    } else {
        pool->tail->next = job;
        pool->tail = job;
    }

    pthread_cond_signal(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void thread_pool_shutdown(LCThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    pthread_mutex_lock(&pool->mutex);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);

    if (pool->threads != NULL) {
        for (size_t i = 0; i < pool->thread_count; ++i) {
            pthread_join(pool->threads[i], NULL);
        }
        free(pool->threads);
        pool->threads = NULL;
    }

    LCThreadPoolJob *job = pool->head;
    while (job != NULL) {
        LCThreadPoolJob *next = job->next;
        free(job);
        job = next;
    }
    pool->head = NULL;
    pool->tail = NULL;

    pthread_cond_destroy(&pool->condition);
    pthread_mutex_destroy(&pool->mutex);
}
