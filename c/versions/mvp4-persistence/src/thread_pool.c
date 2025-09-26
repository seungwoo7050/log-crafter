#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

// [SEQUENCE: 69] Worker thread main loop
void* thread_pool_worker(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    thread_job_t* job;
    
    while (1) {
        // [SEQUENCE: 70] Wait for job or shutdown signal
        pthread_mutex_lock(&pool->queue_mutex);
        
        while (pool->job_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->job_available, &pool->queue_mutex);
        }
        
        if (pool->shutdown && (pool->stop_immediately || pool->job_count == 0)) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // [SEQUENCE: 71] Dequeue job from head
        job = pool->job_queue_head;
        if (job) {
            pool->job_queue_head = job->next;
            if (!pool->job_queue_head) {
                pool->job_queue_tail = NULL;
            }
            pool->job_count--;
            pool->working_threads++;
        }
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // [SEQUENCE: 72] Execute job outside of lock
        if (job) {
            job->function(job->arg);
            free(job);
            
            // [SEQUENCE: 73] Update working thread count
            pthread_mutex_lock(&pool->queue_mutex);
            pool->working_threads--;
            if (pool->working_threads == 0 && pool->job_count == 0) {
                pthread_cond_signal(&pool->all_jobs_done);
            }
            pthread_mutex_unlock(&pool->queue_mutex);
        }
    }
    
    pthread_exit(NULL);
}

// [SEQUENCE: 74] Create thread pool with specified number of threads
thread_pool_t* thread_pool_create(int thread_count) {
    if (thread_count <= 0 || thread_count > MAX_THREAD_COUNT) {
        thread_count = DEFAULT_THREAD_COUNT;
    }
    
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    if (!pool) {
        perror("malloc");
        return NULL;
    }
    
    // [SEQUENCE: 75] Initialize pool structure
    pool->thread_count = thread_count;
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        free(pool);
        perror("malloc");
        return NULL;
    }
    
    pool->job_queue_head = NULL;
    pool->job_queue_tail = NULL;
    pool->job_count = 0;
    pool->shutdown = false;
    pool->stop_immediately = false;
    pool->working_threads = 0;
    
    // [SEQUENCE: 76] Initialize synchronization primitives
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->job_available, NULL) != 0) {
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->all_jobs_done, NULL) != 0) {
        pthread_mutex_destroy(&pool->queue_mutex);
        pthread_cond_destroy(&pool->job_available);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    // [SEQUENCE: 77] Create worker threads
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_pool_worker, pool) != 0) {
            // Cleanup on failure
            pool->shutdown = true;
            pool->stop_immediately = true;
            pthread_cond_broadcast(&pool->job_available);
            
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->job_available);
            pthread_cond_destroy(&pool->all_jobs_done);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

// [SEQUENCE: 78] Add job to thread pool queue
int thread_pool_add_job(thread_pool_t* pool, void (*function)(void*), void* arg) {
    if (!pool || pool->shutdown) {
        return -1;
    }
    
    // [SEQUENCE: 79] Create new job
    thread_job_t* job = malloc(sizeof(thread_job_t));
    if (!job) {
        perror("malloc");
        return -1;
    }
    
    job->function = function;
    job->arg = arg;
    job->next = NULL;
    
    // [SEQUENCE: 80] Enqueue job at tail
    pthread_mutex_lock(&pool->queue_mutex);
    
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_mutex);
        free(job);
        return -1;
    }
    
    if (!pool->job_queue_tail) {
        pool->job_queue_head = job;
        pool->job_queue_tail = job;
    } else {
        pool->job_queue_tail->next = job;
        pool->job_queue_tail = job;
    }
    pool->job_count++;
    
    // [SEQUENCE: 81] Signal waiting worker
    pthread_cond_signal(&pool->job_available);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

// [SEQUENCE: 82] Wait for all jobs to complete
void thread_pool_wait(thread_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->job_count > 0 || pool->working_threads > 0) {
        pthread_cond_wait(&pool->all_jobs_done, &pool->queue_mutex);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
}

// [SEQUENCE: 83] Destroy thread pool and cleanup resources
void thread_pool_destroy(thread_pool_t* pool) {
    if (!pool) return;
    
    // [SEQUENCE: 84] Signal shutdown to all threads
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->job_available);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // [SEQUENCE: 85] Wait for all threads to finish
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // [SEQUENCE: 86] Free remaining jobs
    thread_job_t* job;
    while (pool->job_queue_head) {
        job = pool->job_queue_head;
        pool->job_queue_head = job->next;
        free(job);
    }
    
    // [SEQUENCE: 87] Cleanup synchronization primitives
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->job_available);
    pthread_cond_destroy(&pool->all_jobs_done);
    
    free(pool->threads);
    free(pool);
}
