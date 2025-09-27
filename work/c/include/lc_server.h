/*
 * Sequence: SEQ0024
 * Track: C
 * MVP: mvp5
 * Change: Harden the server configuration with connection limits and guarded metrics for security-focused MVP5.
 * Tests: smoke_security_capacity
 */
#ifndef LC_SERVER_H
#define LC_SERVER_H

#include <limits.h>
#include <stddef.h>

#include <pthread.h>

#include "log_buffer.h"
#include "persistence.h"
#include "thread_pool.h"

#define LC_SERVER_MAX_LOG_LENGTH 1024
#define LC_SERVER_DEFAULT_CAPACITY 10000
#define LC_SERVER_DEFAULT_WORKERS 4
#define LC_SERVER_DEFAULT_PERSISTENCE_DIR "./logs"
#define LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILE_SIZE (10 * 1024 * 1024)
#define LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILES 10
#define LC_SERVER_DEFAULT_MAX_CLIENTS 64

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configuration for the MVP0 bootstrap server.
 */
typedef struct LCServerConfig {
    int log_port;
    int query_port;
    int max_pending_connections;
    int select_timeout_ms;
    size_t buffer_capacity;
    int worker_threads;
    int persistence_enabled;
    size_t persistence_max_file_size;
    size_t persistence_max_files;
    char persistence_directory[PATH_MAX];
    int max_clients;
} LCServerConfig;

/**
 * Runtime state for the MVP0 server.
 */
typedef struct LCServer {
    LCServerConfig config;
    int log_listener_fd;
    int query_listener_fd;
    int running;
    LCThreadPool thread_pool;
    LCLogBuffer log_buffer;
    LCPersistence persistence;
    int active_log_clients;
    int active_query_clients;
    int pending_log_clients;
    int pending_query_clients;
    unsigned long rejected_clients;
    pthread_mutex_t metrics_lock;
    int metrics_initialized;
    int log_buffer_initialized;
    int thread_pool_initialized;
    int persistence_initialized;
} LCServer;

/**
 * Populate an LCServerConfig with default MVP0 values.
 */
LCServerConfig lc_server_config_default(void);

/**
 * Initialize the MVP0 server with the provided configuration.
 * Returns 0 on success or -1 if sockets could not be created.
 */
int lc_server_init(LCServer *server, const LCServerConfig *config);

/**
 * Enter the blocking event loop. Returns 0 on clean shutdown or -1 on fatal error.
 */
int lc_server_run(LCServer *server);

/**
 * Request a graceful shutdown from signal handlers or other control paths.
 */
void lc_server_request_stop(LCServer *server);

/**
 * Close open sockets and release resources.
 */
void lc_server_shutdown(LCServer *server);

#ifdef __cplusplus
}
#endif

#endif /* LC_SERVER_H */
