/*
 * Sequence: SEQ0019
 * Track: C
 * MVP: mvp4
 * Change: Wire the asynchronous persistence manager into the thread-pooled server lifecycle and expose updated stats.
 * Tests: smoke_persistence_rotation
 */
#include "lc_server.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "query_parser.h"

#define LC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct LCServerClientJob {
    LCServer *server;
    int client_fd;
} LCServerClientJob;

static int lc_create_listener(int port, int backlog);
static int lc_accept_client(int listener_fd);
static void lc_log_client_worker(void *arg);
static void lc_query_client_worker(void *arg);
static void lc_log_client_session(LCServer *server, int client_fd);
static void lc_query_client_session(LCServer *server, int client_fd);
static void lc_store_log(LCServer *server, const char *message);
static void lc_send_all(int fd, const char *data, size_t length);
static ssize_t lc_recv_line(int fd, char *buffer, size_t capacity, int *truncated,
                            int *connection_closed);
static void lc_handle_query_line(LCServer *server, int client_fd, const char *line);
static void lc_send_help(int client_fd);
static void lc_send_count(LCServer *server, int client_fd);
static void lc_send_stats(LCServer *server, int client_fd);
static void lc_send_query_advanced(LCServer *server, int client_fd, const char *arguments);
static void lc_send_query_results(int client_fd, char **results, size_t count);
static void lc_send_error_message(int client_fd, const char *message);
static void lc_metrics_log_client_enter(LCServer *server);
static void lc_metrics_log_client_leave(LCServer *server);
static void lc_metrics_query_client_enter(LCServer *server);
static void lc_metrics_query_client_leave(LCServer *server);
static int lc_metrics_total_clients(LCServer *server);
static void lc_persistence_replay(const char *message, time_t timestamp, void *user_data);

LCServerConfig lc_server_config_default(void) {
    LCServerConfig config;
    config.log_port = 9999;
    config.query_port = 9998;
    config.max_pending_connections = 16;
    config.select_timeout_ms = 500;
    config.buffer_capacity = LC_SERVER_DEFAULT_CAPACITY;
    config.worker_threads = LC_SERVER_DEFAULT_WORKERS;
    config.persistence_enabled = 0;
    config.persistence_max_file_size = LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILE_SIZE;
    config.persistence_max_files = LC_SERVER_DEFAULT_PERSISTENCE_MAX_FILES;
    snprintf(config.persistence_directory, sizeof(config.persistence_directory),
             "%s", LC_SERVER_DEFAULT_PERSISTENCE_DIR);
    return config;
}

int lc_server_init(LCServer *server, const LCServerConfig *config) {
    if (server == NULL || config == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(server, 0, sizeof(*server));
    server->config = *config;
    server->running = 1;
    server->log_listener_fd = -1;
    server->query_listener_fd = -1;

    if (pthread_mutex_init(&server->metrics_lock, NULL) != 0) {
        return -1;
    }
    server->metrics_initialized = 1;

    if (server->config.buffer_capacity == 0) {
        server->config.buffer_capacity = LC_SERVER_DEFAULT_CAPACITY;
    }

    if (lc_log_buffer_init(&server->log_buffer, server->config.buffer_capacity) != 0) {
        lc_server_shutdown(server);
        return -1;
    }
    server->log_buffer_initialized = 1;

    if (server->config.worker_threads <= 0) {
        server->config.worker_threads = LC_SERVER_DEFAULT_WORKERS;
    }

    if (thread_pool_init(&server->thread_pool, (size_t)server->config.worker_threads) != 0) {
        lc_server_shutdown(server);
        return -1;
    }
    server->thread_pool_initialized = 1;

    if (server->config.persistence_enabled) {
        if (lc_persistence_init(&server->persistence, server->config.persistence_directory,
                                server->config.persistence_max_file_size,
                                server->config.persistence_max_files) != 0) {
            perror("persistence");
            lc_server_shutdown(server);
            return -1;
        }
        server->persistence_initialized = 1;
        if (lc_persistence_load_existing(&server->persistence, lc_persistence_replay, server) != 0) {
            fprintf(stderr, "[lc][warn] failed to replay persisted logs into buffer\n");
        }
    }

    server->log_listener_fd = lc_create_listener(server->config.log_port,
                                                 server->config.max_pending_connections);
    if (server->log_listener_fd < 0) {
        perror("log listener");
        lc_server_shutdown(server);
        return -1;
    }

    server->query_listener_fd = lc_create_listener(server->config.query_port,
                                                   server->config.max_pending_connections);
    if (server->query_listener_fd < 0) {
        perror("query listener");
        lc_server_shutdown(server);
        return -1;
    }

    const char *persistence_state = server->config.persistence_enabled
                                        ? server->config.persistence_directory
                                        : "disabled";
    fprintf(stderr,
            "[lc][info] MVP4 server initialized (log=%d, query=%d, workers=%d, buffer=%zu, persistence=%s)\n",
            server->config.log_port, server->config.query_port,
            server->config.worker_threads, server->config.buffer_capacity,
            persistence_state);
    return 0;
}

static void lc_close_if_valid(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

void lc_server_shutdown(LCServer *server) {
    if (server == NULL) {
        return;
    }

    lc_close_if_valid(server->log_listener_fd);
    lc_close_if_valid(server->query_listener_fd);
    server->log_listener_fd = -1;
    server->query_listener_fd = -1;
    server->running = 0;

    if (server->thread_pool_initialized) {
        thread_pool_shutdown(&server->thread_pool);
        server->thread_pool_initialized = 0;
    }

    if (server->log_buffer_initialized) {
        lc_log_buffer_destroy(&server->log_buffer);
        server->log_buffer_initialized = 0;
    }

    if (server->persistence_initialized) {
        lc_persistence_shutdown(&server->persistence);
        server->persistence_initialized = 0;
    }

    if (server->metrics_initialized) {
        pthread_mutex_destroy(&server->metrics_lock);
        server->metrics_initialized = 0;
    }

    fprintf(stderr, "[lc][info] MVP4 server shutdown complete\n");
}

void lc_server_request_stop(LCServer *server) {
    if (server != NULL) {
        server->running = 0;
    }
}

static int lc_accept_client(int listener_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
    }
    return client_fd;
}

int lc_server_run(LCServer *server) {
    if (server == NULL) {
        errno = EINVAL;
        return -1;
    }

    int max_fd = server->log_listener_fd > server->query_listener_fd ?
                 server->log_listener_fd : server->query_listener_fd;

    while (server->running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server->log_listener_fd, &readfds);
        FD_SET(server->query_listener_fd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = server->config.select_timeout_ms / 1000;
        timeout.tv_usec = (server->config.select_timeout_ms % 1000) * 1000;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            return -1;
        }

        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(server->log_listener_fd, &readfds)) {
            int client_fd = lc_accept_client(server->log_listener_fd);
            if (client_fd >= 0) {
                LCServerClientJob *job = malloc(sizeof(*job));
                if (job == NULL) {
                    close(client_fd);
                } else {
                    job->server = server;
                    job->client_fd = client_fd;
                    if (thread_pool_submit(&server->thread_pool, lc_log_client_worker, job) != 0) {
                        close(client_fd);
                        free(job);
                    }
                }
            }
        }

        if (FD_ISSET(server->query_listener_fd, &readfds)) {
            int client_fd = lc_accept_client(server->query_listener_fd);
            if (client_fd >= 0) {
                LCServerClientJob *job = malloc(sizeof(*job));
                if (job == NULL) {
                    close(client_fd);
                } else {
                    job->server = server;
                    job->client_fd = client_fd;
                    if (thread_pool_submit(&server->thread_pool, lc_query_client_worker, job) != 0) {
                        close(client_fd);
                        free(job);
                    }
                }
            }
        }
    }

    return 0;
}

static int lc_create_listener(int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, backlog) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static void lc_metrics_log_client_enter(LCServer *server) {
    pthread_mutex_lock(&server->metrics_lock);
    server->active_log_clients++;
    pthread_mutex_unlock(&server->metrics_lock);
}

static void lc_metrics_log_client_leave(LCServer *server) {
    pthread_mutex_lock(&server->metrics_lock);
    if (server->active_log_clients > 0) {
        server->active_log_clients--;
    }
    pthread_mutex_unlock(&server->metrics_lock);
}

static void lc_metrics_query_client_enter(LCServer *server) {
    pthread_mutex_lock(&server->metrics_lock);
    server->active_query_clients++;
    pthread_mutex_unlock(&server->metrics_lock);
}

static void lc_metrics_query_client_leave(LCServer *server) {
    pthread_mutex_lock(&server->metrics_lock);
    if (server->active_query_clients > 0) {
        server->active_query_clients--;
    }
    pthread_mutex_unlock(&server->metrics_lock);
}

static int lc_metrics_total_clients(LCServer *server) {
    pthread_mutex_lock(&server->metrics_lock);
    int total = server->active_log_clients + server->active_query_clients;
    pthread_mutex_unlock(&server->metrics_lock);
    return total;
}

static void lc_persistence_replay(const char *message, time_t timestamp, void *user_data) {
    if (message == NULL || user_data == NULL) {
        return;
    }
    LCServer *server = (LCServer *)user_data;
    time_t effective_timestamp = timestamp != (time_t)0 ? timestamp : time(NULL);
    if (lc_log_buffer_push_with_time(&server->log_buffer, message, effective_timestamp) != 0) {
        fprintf(stderr, "[lc][warn] failed to replay persisted log\n");
    }
}

static void lc_log_client_worker(void *arg) {
    LCServerClientJob *job = (LCServerClientJob *)arg;
    LCServer *server = job->server;
    int client_fd = job->client_fd;
    free(job);

    lc_metrics_log_client_enter(server);
    lc_log_client_session(server, client_fd);
    close(client_fd);
    lc_metrics_log_client_leave(server);
}

static void lc_query_client_worker(void *arg) {
    LCServerClientJob *job = (LCServerClientJob *)arg;
    LCServer *server = job->server;
    int client_fd = job->client_fd;
    free(job);

    lc_metrics_query_client_enter(server);
    lc_query_client_session(server, client_fd);
    close(client_fd);
    lc_metrics_query_client_leave(server);
}

static void lc_trim_trailing(char *buffer) {
    size_t length = strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
        buffer[--length] = '\0';
    }
}

static void lc_log_client_session(LCServer *server, int client_fd) {
    char buffer[LC_SERVER_MAX_LOG_LENGTH + 1];
    const char welcome[] = "LogCrafter MVP4: send newline-terminated log lines.\n";
    lc_send_all(client_fd, welcome, sizeof(welcome) - 1);

    for (;;) {
        int truncated = 0;
        int connection_closed = 0;
        ssize_t length = lc_recv_line(client_fd, buffer, sizeof(buffer), &truncated, &connection_closed);
        if (length < 0) {
            perror("recv");
            break;
        }
        if (length == 0 && connection_closed) {
            break;
        }

        if (truncated) {
            size_t len = strlen(buffer);
            const char ellipsis[] = "...";
            size_t copy_pos = len;
            if (copy_pos + LC_ARRAY_SIZE(ellipsis) > sizeof(buffer)) {
                if (sizeof(buffer) < LC_ARRAY_SIZE(ellipsis) + 1) {
                    buffer[sizeof(buffer) - 1] = '\0';
                } else {
                    copy_pos = sizeof(buffer) - LC_ARRAY_SIZE(ellipsis) - 1;
                }
            }
            strncpy(&buffer[copy_pos], ellipsis, LC_ARRAY_SIZE(ellipsis));
            buffer[sizeof(buffer) - 1] = '\0';
        }

        lc_trim_trailing(buffer);
        if (buffer[0] == '\0' && !truncated && !connection_closed) {
            continue;
        }

        lc_store_log(server, buffer);
        fprintf(stdout, "[lc][log] %s\n", buffer);
        fflush(stdout);

        if (connection_closed) {
            break;
        }
    }
}

static void lc_query_client_session(LCServer *server, int client_fd) {
    char buffer[512];
    const char banner[] =
        "LogCrafter MVP4 query service.\n"
        "Commands: HELP, COUNT, STATS, QUERY keyword=<text> keywords=a,b operator=AND|OR "
        "regex=<pattern> time_from=<unix> time_to=<unix>.\n";
    lc_send_all(client_fd, banner, sizeof(banner) - 1);

    int connection_closed = 0;
    ssize_t length = lc_recv_line(client_fd, buffer, sizeof(buffer), NULL, &connection_closed);
    if (length < 0) {
        perror("recv");
        return;
    }
    if ((length == 0 && connection_closed) || buffer[0] == '\0') {
        return;
    }

    lc_trim_trailing(buffer);
    lc_handle_query_line(server, client_fd, buffer);
}

static void lc_handle_query_line(LCServer *server, int client_fd, const char *line) {
    if (strcmp(line, "HELP") == 0) {
        lc_send_help(client_fd);
    } else if (strcmp(line, "COUNT") == 0) {
        lc_send_count(server, client_fd);
    } else if (strcmp(line, "STATS") == 0) {
        lc_send_stats(server, client_fd);
    } else if (strncmp(line, "QUERY", 5) == 0) {
        const char *arguments = line + 5;
        lc_send_query_advanced(server, client_fd, arguments);
    } else {
        const char error[] = "ERROR: Unknown command. Use HELP for usage.\n";
        lc_send_all(client_fd, error, sizeof(error) - 1);
    }
}

static void lc_send_help(int client_fd) {
    const char help[] =
        "HELP - show this text\n"
        "COUNT - number of logs currently buffered\n"
        "STATS - total processed, dropped, and active client count\n"
        "QUERY keyword=<text> keywords=a,b operator=AND|OR regex=<pattern> "
        "time_from=<unix> time_to=<unix>\n";
    lc_send_all(client_fd, help, sizeof(help) - 1);
}

static void lc_send_count(LCServer *server, int client_fd) {
    size_t current = lc_log_buffer_count(&server->log_buffer);
    char response[64];
    int written = snprintf(response, sizeof(response), "COUNT: %zu\n", current);
    if (written > 0) {
        lc_send_all(client_fd, response, (size_t)written);
    }
}

static void lc_send_stats(LCServer *server, int client_fd) {
    LCLogBufferStats stats;
    lc_log_buffer_get_stats(&server->log_buffer, &stats);
    int clients = lc_metrics_total_clients(server);
    LCPersistenceStats persistence_stats;
    memset(&persistence_stats, 0, sizeof(persistence_stats));
    if (server->config.persistence_enabled && server->persistence_initialized) {
        lc_persistence_get_stats(&server->persistence, &persistence_stats);
    }

    char response[256];
    int written = snprintf(response, sizeof(response),
                           "STATS: Total=%lu, Dropped=%lu, Persisted=%lu, Failed=%lu, Current=%zu, Clients=%d\n",
                           stats.total_logs, stats.dropped_logs, persistence_stats.persisted_logs,
                           persistence_stats.failed_logs, stats.current_size, clients);
    if (written > 0) {
        lc_send_all(client_fd, response, (size_t)written);
    }
}

static void lc_send_query_advanced(LCServer *server, int client_fd, const char *arguments) {
    LCQueryRequest request;
    if (lc_query_request_init(&request) != 0) {
        lc_send_error_message(client_fd, "ERROR: Internal query failure.");
        return;
    }

    char error_buffer[256];
    if (lc_query_parse_arguments(arguments, &request, error_buffer, sizeof(error_buffer)) != 0) {
        const char *base = error_buffer[0] != '\0' ? error_buffer : "ERROR: Invalid query syntax.";
        char normalized[512];
        if (strncmp(base, "ERROR:", 6) != 0) {
            snprintf(normalized, sizeof(normalized), "ERROR: %s", base);
            base = normalized;
        }
        lc_send_error_message(client_fd, base);
        lc_query_request_reset(&request);
        return;
    }

    char **results = NULL;
    size_t count = 0;
    if (lc_log_buffer_execute_query(&server->log_buffer, &request, &results, &count) != 0) {
        lc_send_error_message(client_fd, "ERROR: Search failed.");
        lc_query_request_reset(&request);
        return;
    }

    char header[64];
    int written = snprintf(header, sizeof(header), "FOUND: %zu\n", count);
    if (written > 0) {
        lc_send_all(client_fd, header, (size_t)written);
    }

    lc_send_query_results(client_fd, results, count);

    if (results != NULL) {
        for (size_t i = 0; i < count; ++i) {
            free(results[i]);
        }
        free(results);
    }
    lc_query_request_reset(&request);
}

static void lc_send_query_results(int client_fd, char **results, size_t count) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (results[i] == NULL) {
            continue;
        }
        size_t length = strlen(results[i]);
        if (length > 0) {
            lc_send_all(client_fd, results[i], length);
        }
        lc_send_all(client_fd, "\n", 1);
    }
}

static void lc_send_error_message(int client_fd, const char *message) {
    if (message == NULL) {
        message = "ERROR: Internal server error.";
    }
    size_t length = strlen(message);
    if (length > 0) {
        lc_send_all(client_fd, message, length);
    }
    if (length == 0 || message[length - 1] != '\n') {
        lc_send_all(client_fd, "\n", 1);
    }
}

static void lc_store_log(LCServer *server, const char *message) {
    time_t now = time(NULL);
    if (lc_log_buffer_push_with_time(&server->log_buffer, message, now) != 0) {
        fprintf(stderr, "[lc][warn] failed to store log entry\n");
    }

    if (server->config.persistence_enabled && server->persistence_initialized) {
        if (lc_persistence_enqueue(&server->persistence, message, now) != 0) {
            fprintf(stderr, "[lc][warn] failed to enqueue log for persistence\n");
        }
    }
}

static void lc_send_all(int fd, const char *data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(fd, data + total_sent, length - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            break;
        }
        total_sent += (size_t)sent;
    }
}

static ssize_t lc_recv_line(int fd, char *buffer, size_t capacity, int *truncated,
                            int *connection_closed) {
    size_t offset = 0;
    int local_truncated = 0;
    int closed = 0;

    if (capacity == 0) {
        errno = EINVAL;
        return -1;
    }

    while (1) {
        char ch;
        ssize_t received = recv(fd, &ch, 1, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            closed = 1;
            break;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            break;
        }

        if (offset + 1 < capacity) {
            buffer[offset++] = ch;
        } else {
            local_truncated = 1;
        }
    }

    buffer[offset] = '\0';

    if (truncated != NULL) {
        *truncated = local_truncated;
    }
    if (connection_closed != NULL) {
        *connection_closed = closed;
    }

    return (ssize_t)offset;
}
