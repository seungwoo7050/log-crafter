#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

#define DEFAULT_THREAD_COUNT 4
#define MAX_THREAD_COUNT 32

// [SEQUENCE: 61] Job structure for thread pool tasks
typedef struct thread_job {
    void (*function)(void* arg);
    void* arg;
    struct thread_job* next;
} thread_job_t;

// [SEQUENCE: 62] Thread pool structure with job queue
typedef struct {
    pthread_t* threads;
    int thread_count;
    
    // [SEQUENCE: 63] Job queue management
    thread_job_t* job_queue_head;
    thread_job_t* job_queue_tail;
    int job_count;
    
    // [SEQUENCE: 64] Synchronization primitives
    pthread_mutex_t queue_mutex;
    pthread_cond_t job_available;
    pthread_cond_t all_jobs_done;
    
    // [SEQUENCE: 65] Pool state management
    volatile bool shutdown;
    volatile bool stop_immediately;
    int working_threads;
} thread_pool_t;

// [SEQUENCE: 66] Thread pool lifecycle functions
thread_pool_t* thread_pool_create(int thread_count);
void thread_pool_destroy(thread_pool_t* pool);

// [SEQUENCE: 67] Job submission and execution
int thread_pool_add_job(thread_pool_t* pool, void (*function)(void*), void* arg);
void thread_pool_wait(thread_pool_t* pool);

// [SEQUENCE: 68] Internal worker thread function
void* thread_pool_worker(void* arg);

#endif // THREAD_POOL_H
