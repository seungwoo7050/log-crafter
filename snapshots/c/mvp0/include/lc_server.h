/*
 * Sequence: SEQ0000
 * Track: C
 * MVP: mvp0
 * Change: Introduce the MVP0 bootstrap server skeleton with configuration helpers.
 * Tests: manual_smoke_connect
 */
#ifndef LC_SERVER_H
#define LC_SERVER_H

#include <stddef.h>

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
