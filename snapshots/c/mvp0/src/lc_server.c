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
static void lc_log_client_session(int client_fd);
static void lc_query_client_session(int client_fd);

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

    fprintf(stderr, "[lc][info] MVP0 server initialized (log=%d, query=%d)\n",
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
    fprintf(stderr, "[lc][info] MVP0 server shutdown complete\n");
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
                lc_log_client_session(client_fd);
                close(client_fd);
            }
        }

        if (FD_ISSET(server->query_listener_fd, &readfds)) {
            int client_fd = lc_accept_client(server->query_listener_fd);
            if (client_fd >= 0) {
                lc_query_client_session(client_fd);
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

static void lc_trim_newline(char *buffer, ssize_t length) {
    if (length <= 0) {
        return;
    }
    if (buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        buffer[length] = '\0';
    }
}

static void lc_log_client_session(int client_fd) {
    char buffer[1024];
    const char welcome[] = "MVP0 LogCrafter: send lines to log.\n";
    send(client_fd, welcome, sizeof(welcome) - 1, 0);

    for (;;) {
        ssize_t received = recv(client_fd, buffer, (int)LC_ARRAY_SIZE(buffer) - 1, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            break;
        }
        if (received == 0) {
            break; /* client closed */
        }
        lc_trim_newline(buffer, received);
        fprintf(stdout, "[lc][log] %s\n", buffer);
        fflush(stdout);
    }
}

static void lc_query_client_session(int client_fd) {
    const char response[] =
        "MVP0 LogCrafter query port.\n"
        "Available commands: HELP (returns this message).\n";
    send(client_fd, response, sizeof(response) - 1, 0);
}
