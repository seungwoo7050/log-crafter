/*
 * Sequence: SEQ0039
 * Track: C++
 * MVP: mvp2
 * Change: Upgrade the C++ server to use the shared log buffer and worker pool for concurrent client handling.
 * Tests: smoke_cpp_mvp2_thread_pool
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
#include <vector>

namespace logcrafter::cpp {

namespace {

constexpr int kDefaultLogPort = 9999;
constexpr int kDefaultQueryPort = 9998;
constexpr int kDefaultBacklog = 32;
constexpr int kDefaultTimeoutMs = 500;
constexpr std::size_t kQueryBufferSize = 512;

class FileDescriptorGuard {
public:
    explicit FileDescriptorGuard(int fd) : fd_(fd) {}
    ~FileDescriptorGuard() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    FileDescriptorGuard(const FileDescriptorGuard &) = delete;
    FileDescriptorGuard &operator=(const FileDescriptorGuard &) = delete;

private:
    int fd_;
};

class ActiveClientGuard {
public:
    explicit ActiveClientGuard(std::atomic<int> &counter) : counter_(counter) {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }
    ~ActiveClientGuard() {
        counter_.fetch_sub(1, std::memory_order_relaxed);
    }

    ActiveClientGuard(const ActiveClientGuard &) = delete;
    ActiveClientGuard &operator=(const ActiveClientGuard &) = delete;

private:
    std::atomic<int> &counter_;
};

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
    config.buffer_capacity = Server::kDefaultLogCapacity;
    config.worker_threads = Server::kDefaultWorkerThreads;
    return config;
}

Server::Server()
    : config_(default_config()),
      log_listener_fd_(-1),
      query_listener_fd_(-1),
      running_(false),
      log_buffer_(),
      active_log_clients_(0),
      active_query_clients_(0) {
    log_buffer_.configure(kDefaultLogCapacity);
}

int Server::init(const ServerConfig &config) {
    shutdown();

    config_ = config;
    if (config_.buffer_capacity == 0) {
        config_.buffer_capacity = kDefaultLogCapacity;
    }
    if (config_.worker_threads <= 0) {
        config_.worker_threads = kDefaultWorkerThreads;
    }

    log_buffer_.configure(config_.buffer_capacity);
    active_log_clients_.store(0, std::memory_order_relaxed);
    active_query_clients_.store(0, std::memory_order_relaxed);

    log_listener_fd_ = create_listener(config_.log_port, config_.max_pending_connections);
    if (log_listener_fd_ < 0) {
        std::perror("log listener");
        running_.store(false, std::memory_order_release);
        return -1;
    }

    query_listener_fd_ = create_listener(config_.query_port, config_.max_pending_connections);
    if (query_listener_fd_ < 0) {
        std::perror("query listener");
        ::close(log_listener_fd_);
        log_listener_fd_ = -1;
        running_.store(false, std::memory_order_release);
        return -1;
    }

    if (thread_pool_.start(static_cast<std::size_t>(config_.worker_threads)) != 0) {
        std::cerr << "[lc][error] Failed to start worker pool" << std::endl;
        ::close(log_listener_fd_);
        ::close(query_listener_fd_);
        log_listener_fd_ = -1;
        query_listener_fd_ = -1;
        running_.store(false, std::memory_order_release);
        return -1;
    }

    running_.store(true, std::memory_order_release);
    std::cerr << "[lc][info] MVP2 C++ server initialized (log=" << config_.log_port
              << ", query=" << config_.query_port
              << ", workers=" << config_.worker_threads << ")" << std::endl;
    return 0;
}

void Server::shutdown() {
    running_.store(false, std::memory_order_release);
    thread_pool_.stop();
    if (log_listener_fd_ >= 0) {
        ::close(log_listener_fd_);
        log_listener_fd_ = -1;
    }
    if (query_listener_fd_ >= 0) {
        ::close(query_listener_fd_);
        query_listener_fd_ = -1;
    }
    log_buffer_.reset();
    active_log_clients_.store(0, std::memory_order_relaxed);
    active_query_clients_.store(0, std::memory_order_relaxed);
}

void Server::request_stop() {
    running_.store(false, std::memory_order_release);
}

int Server::run() {
    if (log_listener_fd_ < 0 || query_listener_fd_ < 0) {
        errno = EINVAL;
        return -1;
    }

    while (running_.load(std::memory_order_acquire)) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(log_listener_fd_, &read_fds);
        FD_SET(query_listener_fd_, &read_fds);

        int max_fd = std::max(log_listener_fd_, query_listener_fd_);

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
                dispatch_log_client(client_fd);
            } else if (errno != EINTR) {
                std::perror("accept");
            }
        }

        if (FD_ISSET(query_listener_fd_, &read_fds)) {
            struct sockaddr_in client_addr {};
            socklen_t addr_len = sizeof(client_addr);
            const int client_fd = ::accept(query_listener_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);
            if (client_fd >= 0) {
                dispatch_query_client(client_fd);
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

void Server::dispatch_log_client(int client_fd) {
    if (!thread_pool_.enqueue([this, client_fd]() {
            FileDescriptorGuard guard(client_fd);
            handle_log_client(client_fd);
        })) {
        ::close(client_fd);
    }
}

void Server::dispatch_query_client(int client_fd) {
    if (!thread_pool_.enqueue([this, client_fd]() {
            FileDescriptorGuard guard(client_fd);
            handle_query_client(client_fd);
        })) {
        ::close(client_fd);
    }
}

void Server::store_log(const std::string &message) {
    log_buffer_.push(message);
}

void Server::handle_log_client(int client_fd) {
    ActiveClientGuard guard(active_log_clients_);

    const char welcome[] = "LogCrafter C++ MVP2: send newline-terminated log lines.\n";
    send_all(client_fd, welcome, sizeof(welcome) - 1);

    char buffer[kMaxLogLength + 1];
    while (running_.load(std::memory_order_acquire)) {
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
    ActiveClientGuard guard(active_query_clients_);

    const char banner[] =
        "LogCrafter C++ MVP2 query service.\n"
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
        "STATS - total processed, dropped, and active client counts\n"
        "QUERY keyword=<text> - find logs containing <text>\n";
    send_all(client_fd, help, sizeof(help) - 1);
}

void Server::send_count(int client_fd) const {
    LogBufferStats stats = log_buffer_.stats();
    std::ostringstream oss;
    oss << "COUNT: " << stats.current_size << "\n";
    send_all(client_fd, oss.str());
}

void Server::send_stats(int client_fd) const {
    LogBufferStats stats = log_buffer_.stats();
    std::ostringstream oss;
    oss << "STATS: Total=" << stats.total_logs
        << ", Dropped=" << stats.dropped_logs
        << ", Current=" << stats.current_size
        << ", ActiveLog=" << active_log_clients_.load(std::memory_order_relaxed)
        << ", ActiveQuery=" << active_query_clients_.load(std::memory_order_relaxed)
        << "\n";
    send_all(client_fd, oss.str());
}

void Server::send_query_keyword(int client_fd, const std::string &keyword) const {
    std::vector<std::string> logs = log_buffer_.snapshot();
    std::vector<const std::string *> matches;
    matches.reserve(logs.size());

    for (const std::string &entry : logs) {
        if (entry.find(keyword) != std::string::npos) {
            matches.push_back(&entry);
        }
    }

    std::ostringstream header;
    header << "FOUND: " << matches.size() << "\n";
    send_all(client_fd, header.str());

    for (const std::string *entry : matches) {
        send_all(client_fd, *entry);
        send_all(client_fd, "\n", 1);
    }
}

} // namespace logcrafter::cpp
