/*
 * Sequence: SEQ0002
 * Track: C
 * MVP: mvp1
 * Change: Implement MVP1 in-memory log storage and query handling on the single-threaded server.
 * Tests: smoke_log_store_query
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
#include <unistd.h>

#define LC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static int lc_create_listener(int port, int backlog);
static void lc_log_client_session(LCServer *server, int client_fd);
static void lc_query_client_session(LCServer *server, int client_fd);
static void lc_store_log(LCServer *server, const char *message);
static void lc_send_all(int fd, const char *data, size_t length);
static ssize_t lc_recv_line(int fd, char *buffer, size_t capacity, int *truncated, int *connection_closed);
static void lc_handle_query_line(LCServer *server, int client_fd, const char *line);
static void lc_send_help(int client_fd);
static void lc_send_count(const LCServer *server, int client_fd);
static void lc_send_stats(const LCServer *server, int client_fd);
static void lc_send_query_keyword(const LCServer *server, int client_fd, const char *keyword);
static size_t lc_logs_oldest_index(const LCServer *server);

LCServerConfig lc_server_config_default(void) {
    LCServerConfig config;
    config.log_port = 9999;
    config.query_port = 9998;
    config.max_pending_connections = 16;
    config.select_timeout_ms = 500;
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
    server->max_logs = LC_SERVER_DEFAULT_CAPACITY;
    server->log_size = 0;
    server->log_head = 0;
    server->total_logs = 0;
    server->dropped_logs = 0;

    server->log_listener_fd = lc_create_listener(server->config.log_port, server->config.max_pending_connections);
    if (server->log_listener_fd < 0) {
        perror("log listener");
        return -1;
    }

    server->query_listener_fd = lc_create_listener(server->config.query_port, server->config.max_pending_connections);
    if (server->query_listener_fd < 0) {
        perror("query listener");
        close(server->log_listener_fd);
        server->log_listener_fd = -1;
        return -1;
    }

    fprintf(stderr, "[lc][info] MVP1 server initialized (log=%d, query=%d)\n",
            server->config.log_port, server->config.query_port);
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
    fprintf(stderr, "[lc][info] MVP1 server shutdown complete\n");
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
            continue; /* timeout */
        }

        if (FD_ISSET(server->log_listener_fd, &readfds)) {
            int client_fd = lc_accept_client(server->log_listener_fd);
            if (client_fd >= 0) {
                lc_log_client_session(server, client_fd);
                close(client_fd);
            }
        }

        if (FD_ISSET(server->query_listener_fd, &readfds)) {
            int client_fd = lc_accept_client(server->query_listener_fd);
            if (client_fd >= 0) {
                lc_query_client_session(server, client_fd);
                close(client_fd);
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

static void lc_store_log(LCServer *server, const char *message) {
    if (server->max_logs == 0) {
        return;
    }

    size_t index = server->log_head;
    strncpy(server->logs[index], message, LC_SERVER_MAX_LOG_LENGTH);
    server->logs[index][LC_SERVER_MAX_LOG_LENGTH] = '\0';

    server->log_head = (server->log_head + 1) % server->max_logs;
    if (server->log_size == server->max_logs) {
        server->dropped_logs++;
    } else {
        server->log_size++;
    }

    server->total_logs++;
}

static void lc_trim_trailing(char *buffer) {
    size_t length = strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
        buffer[--length] = '\0';
    }
}

static void lc_log_client_session(LCServer *server, int client_fd) {
    char buffer[LC_SERVER_MAX_LOG_LENGTH + 1];
    const char welcome[] = "LogCrafter MVP1: send newline-terminated log lines.\n";
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
            break; /* client closed */
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
            continue; /* ignore blank lines */
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
        "LogCrafter MVP1 query service.\n"
        "Commands: HELP, COUNT, STATS, QUERY keyword=<text>.\n";
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
        const char *keyword_token = strstr(line, "keyword=");
        if (keyword_token == NULL) {
            const char error[] = "ERROR: Missing keyword parameter.\n";
            lc_send_all(client_fd, error, sizeof(error) - 1);
            return;
        }
        keyword_token += strlen("keyword=");
        if (*keyword_token == '\0') {
            const char error[] = "ERROR: Empty keyword parameter.\n";
            lc_send_all(client_fd, error, sizeof(error) - 1);
            return;
        }
        lc_send_query_keyword(server, client_fd, keyword_token);
    } else {
        const char error[] = "ERROR: Unknown command. Use HELP for usage.\n";
        lc_send_all(client_fd, error, sizeof(error) - 1);
    }
}

static void lc_send_help(int client_fd) {
    const char help[] =
        "HELP - show this text\n"
        "COUNT - number of logs currently buffered\n"
        "STATS - total processed and dropped counts\n"
        "QUERY keyword=<text> - find logs containing <text>\n";
    lc_send_all(client_fd, help, sizeof(help) - 1);
}

static void lc_send_count(const LCServer *server, int client_fd) {
    char response[64];
    int written = snprintf(response, sizeof(response), "COUNT: %zu\n", server->log_size);
    if (written > 0) {
        lc_send_all(client_fd, response, (size_t)written);
    }
}

static void lc_send_stats(const LCServer *server, int client_fd) {
    char response[128];
    int written = snprintf(response, sizeof(response),
                           "STATS: Total=%lu, Dropped=%lu, Current=%zu\n",
                           server->total_logs, server->dropped_logs, server->log_size);
    if (written > 0) {
        lc_send_all(client_fd, response, (size_t)written);
    }
}

static void lc_send_query_keyword(const LCServer *server, int client_fd, const char *keyword) {
    size_t matches = 0;
    size_t start = lc_logs_oldest_index(server);
    size_t index = start;
    for (size_t i = 0; i < server->log_size; ++i) {
        if (strstr(server->logs[index], keyword) != NULL) {
            matches++;
        }
        index = (index + 1) % server->max_logs;
    }

    char header[64];
    int written = snprintf(header, sizeof(header), "FOUND: %zu\n", matches);
    if (written > 0) {
        lc_send_all(client_fd, header, (size_t)written);
    }

    if (matches == 0) {
        return;
    }

    index = start;
    for (size_t i = 0; i < server->log_size; ++i) {
        if (strstr(server->logs[index], keyword) != NULL) {
            lc_send_all(client_fd, server->logs[index], strlen(server->logs[index]));
            lc_send_all(client_fd, "\n", 1);
        }
        index = (index + 1) % server->max_logs;
    }
}

static size_t lc_logs_oldest_index(const LCServer *server) {
    if (server->log_size == 0) {
        return 0;
    }
    if (server->log_head >= server->log_size) {
        return server->log_head - server->log_size;
    }
    return server->max_logs - (server->log_size - server->log_head);
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

static ssize_t lc_recv_line(int fd, char *buffer, size_t capacity, int *truncated, int *connection_closed) {
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
