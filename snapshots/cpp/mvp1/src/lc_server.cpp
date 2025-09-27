/*
 * Sequence: SEQ0032
 * Track: C++
 * MVP: mvp1
 * Change: Implement MVP1 in-memory log storage, COUNT/STATS reporting, and keyword query streaming.
 * Tests: smoke_cpp_mvp1_query
 */
#include "lc_server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
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
constexpr std::size_t kQueryBufferSize = 512;

void send_all(int fd, const char *data, std::size_t length) {
    std::size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = ::send(fd, data + total_sent, length - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("send");
            break;
        }
        total_sent += static_cast<std::size_t>(sent);
    }
}

void send_all(int fd, const std::string &text) {
    send_all(fd, text.c_str(), text.size());
}

ssize_t recv_line(int fd, char *buffer, std::size_t capacity, bool &truncated, bool &connection_closed) {
    truncated = false;
    connection_closed = false;
    std::size_t total = 0;

    while (true) {
        char ch = 0;
        ssize_t received = ::recv(fd, &ch, 1, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            connection_closed = true;
            break;
        }
        if (ch == '\n') {
            break;
        }
        if (total < capacity - 1) {
            buffer[total++] = ch;
        } else {
            truncated = true;
        }
    }

    buffer[total] = '\0';
    return static_cast<ssize_t>(total);
}

void trim_trailing(std::string &line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
}

void apply_ellipsis(std::string &line, std::size_t max_length) {
    static constexpr const char ellipsis[] = "...";
    if (line.size() > max_length) {
        if (max_length >= sizeof(ellipsis) - 1) {
            line.resize(max_length - (sizeof(ellipsis) - 1));
            line.append(ellipsis, sizeof(ellipsis) - 1);
        } else {
            line.resize(max_length);
        }
        return;
    }

    if (line.size() + (sizeof(ellipsis) - 1) > max_length) {
        if (max_length >= sizeof(ellipsis) - 1) {
            line.resize(max_length - (sizeof(ellipsis) - 1));
            line.append(ellipsis, sizeof(ellipsis) - 1);
        }
    } else {
        line.append(ellipsis, sizeof(ellipsis) - 1);
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
      running_(false),
      logs_(kDefaultLogCapacity),
      log_capacity_(kDefaultLogCapacity),
      log_size_(0),
      log_head_(0),
      total_logs_(0),
      dropped_logs_(0) {}

int Server::init(const ServerConfig &config) {
    shutdown();

    config_ = config;
    running_.store(true);

    logs_.assign(log_capacity_, std::string());
    log_size_ = 0;
    log_head_ = 0;
    total_logs_ = 0;
    dropped_logs_ = 0;

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

    std::cerr << "[lc][info] MVP1 C++ server initialized (log=" << config_.log_port
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
            struct sockaddr_in client_addr {};
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
            struct sockaddr_in client_addr {};
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

void Server::store_log(const std::string &message) {
    if (log_capacity_ == 0) {
        return;
    }

    logs_[log_head_] = message;
    log_head_ = (log_head_ + 1) % log_capacity_;
    if (log_size_ == log_capacity_) {
        ++dropped_logs_;
    } else {
        ++log_size_;
    }
    ++total_logs_;
}

void Server::handle_log_client(int client_fd) {
    const char welcome[] = "LogCrafter C++ MVP1: send newline-terminated log lines.\n";
    send_all(client_fd, welcome, sizeof(welcome) - 1);

    char buffer[kMaxLogLength + 1];
    while (running_.load()) {
        bool truncated = false;
        bool connection_closed = false;
        ssize_t length = recv_line(client_fd, buffer, sizeof(buffer), truncated, connection_closed);
        if (length < 0) {
            std::perror("recv");
            break;
        }
        if (length == 0 && connection_closed) {
            break;
        }

        std::string line(buffer, static_cast<std::size_t>(length));
        trim_trailing(line);

        if (truncated) {
            apply_ellipsis(line, kMaxLogLength);
        }

        if (line.empty() && !truncated && !connection_closed) {
            if (connection_closed) {
                break;
            }
            continue;
        }

        store_log(line);
        std::cout << "[lc][log] " << line << std::endl;

        if (connection_closed) {
            break;
        }
    }
}

void Server::handle_query_client(int client_fd) {
    const char banner[] =
        "LogCrafter C++ MVP1 query service.\n"
        "Commands: HELP, COUNT, STATS, QUERY keyword=<text>.\n";
    send_all(client_fd, banner, sizeof(banner) - 1);

    char buffer[kQueryBufferSize];
    bool truncated = false;
    bool connection_closed = false;
    ssize_t length = recv_line(client_fd, buffer, sizeof(buffer), truncated, connection_closed);
    if (length < 0) {
        std::perror("recv");
        return;
    }
    if ((length == 0 && connection_closed) || buffer[0] == '\0') {
        return;
    }

    std::string line(buffer, static_cast<std::size_t>(length));
    trim_trailing(line);

    if (line == "HELP") {
        send_help(client_fd);
    } else if (line == "COUNT") {
        send_count(client_fd);
    } else if (line == "STATS") {
        send_stats(client_fd);
    } else if (line.rfind("QUERY", 0) == 0) {
        const std::string keyword_token = "keyword=";
        std::size_t keyword_pos = line.find(keyword_token);
        if (keyword_pos == std::string::npos) {
            send_all(client_fd, "ERROR: Missing keyword parameter.\n");
            return;
        }
        keyword_pos += keyword_token.size();
        if (keyword_pos >= line.size()) {
            send_all(client_fd, "ERROR: Empty keyword parameter.\n");
            return;
        }
        std::string keyword = line.substr(keyword_pos);
        if (keyword.empty()) {
            send_all(client_fd, "ERROR: Empty keyword parameter.\n");
            return;
        }
        send_query_keyword(client_fd, keyword);
    } else {
        send_all(client_fd, "ERROR: Unknown command. Use HELP for usage.\n");
    }
}

void Server::send_help(int client_fd) const {
    const char help[] =
        "HELP - show this text\n"
        "COUNT - number of logs currently buffered\n"
        "STATS - total processed and dropped counts\n"
        "QUERY keyword=<text> - find logs containing <text>\n";
    send_all(client_fd, help, sizeof(help) - 1);
}

void Server::send_count(int client_fd) const {
    std::ostringstream oss;
    oss << "COUNT: " << log_size_ << "\n";
    send_all(client_fd, oss.str());
}

void Server::send_stats(int client_fd) const {
    std::ostringstream oss;
    oss << "STATS: Total=" << total_logs_
        << ", Dropped=" << dropped_logs_
        << ", Current=" << log_size_ << "\n";
    send_all(client_fd, oss.str());
}

void Server::send_query_keyword(int client_fd, const std::string &keyword) const {
    std::size_t matches = 0;
    if (log_size_ == 0) {
        send_all(client_fd, "FOUND: 0\n");
        return;
    }

    std::size_t start_index = (log_size_ == log_capacity_) ? log_head_ : 0;
    for (std::size_t i = 0; i < log_size_; ++i) {
        std::size_t index = (start_index + i) % log_capacity_;
        if (logs_[index].find(keyword) != std::string::npos) {
            ++matches;
        }
    }

    std::ostringstream header;
    header << "FOUND: " << matches << "\n";
    send_all(client_fd, header.str());

    if (matches == 0) {
        return;
    }

    for (std::size_t i = 0; i < log_size_; ++i) {
        std::size_t index = (start_index + i) % log_capacity_;
        if (logs_[index].find(keyword) != std::string::npos) {
            send_all(client_fd, logs_[index]);
            send_all(client_fd, "\n", 1);
        }
    }
}

} // namespace logcrafter::cpp
