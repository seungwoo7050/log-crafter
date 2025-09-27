/*
 * Sequence: SEQ0001
 * Track: C
 * MVP: mvp1
 * Change: Expand the MVP1 server contract with in-memory log storage and query support.
 * Tests: smoke_log_store_query
 */
#ifndef LC_SERVER_H
#define LC_SERVER_H

#include <stddef.h>

#define LC_SERVER_MAX_LOG_LENGTH 1024
#define LC_SERVER_DEFAULT_CAPACITY 10000

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
} LCServerConfig;

/**
 * Runtime state for the MVP0 server.
 */
typedef struct LCServer {
    LCServerConfig config;
    int log_listener_fd;
    int query_listener_fd;
    int running;
    size_t max_logs;
    size_t log_size;
    size_t log_head;
    unsigned long total_logs;
    unsigned long dropped_logs;
    char logs[LC_SERVER_DEFAULT_CAPACITY][LC_SERVER_MAX_LOG_LENGTH + 1];
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
