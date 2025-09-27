#include "lc_server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace logcrafter::cpp {

namespace {

constexpr int kDefaultLogPort = 9999;
constexpr int kDefaultQueryPort = 9998;
constexpr int kDefaultBacklog = 16;
constexpr int kDefaultTimeoutMs = 500;

void trim_newline(char *buffer, ssize_t length) {
    if (length <= 0) {
        return;
    }
    if (buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        buffer[length] = '\0';
    }
}

} // namespace

ServerConfig default_config() {
    ServerConfig config{};
    config.log_port = kDefaultLogPort;
    config.query_port = kDefaultQueryPort;
    config.max_pending_connections = kDefaultBacklog;
    config.select_timeout_ms = kDefaultTimeoutMs;
    return config;
}

Server::Server()
    : config_(default_config()),
      log_listener_fd_(-1),
      query_listener_fd_(-1),
      running_(false) {}

int Server::init(const ServerConfig &config) {
    shutdown();

    config_ = config;
    running_.store(true);

    log_listener_fd_ = create_listener(config_.log_port, config_.max_pending_connections);
    if (log_listener_fd_ < 0) {
        std::perror("log listener");
        running_.store(false);
        return -1;
    }

    query_listener_fd_ = create_listener(config_.query_port, config_.max_pending_connections);
    if (query_listener_fd_ < 0) {
        std::perror("query listener");
        ::close(log_listener_fd_);
        log_listener_fd_ = -1;
        running_.store(false);
        return -1;
    }

    std::cerr << "[lc][info] MVP0 C++ server initialized (log=" << config_.log_port
              << ", query=" << config_.query_port << ")" << std::endl;
    return 0;
}

void Server::shutdown() {
    if (log_listener_fd_ >= 0) {
        ::close(log_listener_fd_);
        log_listener_fd_ = -1;
    }
    if (query_listener_fd_ >= 0) {
        ::close(query_listener_fd_);
        query_listener_fd_ = -1;
    }
    running_.store(false);
}

void Server::request_stop() {
    running_.store(false);
}

int Server::run() {
    if (log_listener_fd_ < 0 || query_listener_fd_ < 0) {
        errno = EINVAL;
        return -1;
    }

    const int max_fd = std::max(log_listener_fd_, query_listener_fd_);

    while (running_.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(log_listener_fd_, &read_fds);
        FD_SET(query_listener_fd_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = config_.select_timeout_ms / 1000;
        timeout.tv_usec = (config_.select_timeout_ms % 1000) * 1000;

        const int ready = ::select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("select");
            return -1;
        }

        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(log_listener_fd_, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            const int client_fd = ::accept(log_listener_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);
            if (client_fd >= 0) {
                handle_log_client(client_fd);
                ::close(client_fd);
            } else if (errno != EINTR) {
                std::perror("accept");
            }
        }

        if (FD_ISSET(query_listener_fd_, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            const int client_fd = ::accept(query_listener_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);
            if (client_fd >= 0) {
                handle_query_client(client_fd);
                ::close(client_fd);
            } else if (errno != EINTR) {
                std::perror("accept");
            }
        }
    }

    return 0;
}

int Server::create_listener(int port, int backlog) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    const int optval = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::perror("setsockopt");
        ::close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(fd);
        return -1;
    }

    if (::listen(fd, backlog) < 0) {
        std::perror("listen");
        ::close(fd);
        return -1;
    }

    return fd;
}

void Server::handle_log_client(int client_fd) {
    char buffer[1024];
    const char welcome[] = "MVP0 LogCrafter C++: send lines to log.\n";
    ::send(client_fd, welcome, sizeof(welcome) - 1, 0);

    while (running_.load()) {
        const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("recv");
            break;
        }
        if (received == 0) {
            break;
        }
        trim_newline(buffer, received);
        std::cout << "[lc][log] " << buffer << std::endl;
    }
}

void Server::handle_query_client(int client_fd) {
    const char response[] =
        "MVP0 LogCrafter C++ query port.\n"
        "Available commands: HELP (returns this message).\n";
    ::send(client_fd, response, sizeof(response) - 1, 0);
}

} // namespace logcrafter::cpp
