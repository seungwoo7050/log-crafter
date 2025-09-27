/*
 * Sequence: SEQ0062
 * Track: C++
 * MVP: mvp5
 * Change: Implement the standalone IRC server thread that accepts clients and streams log entries to channels.
 * Tests: smoke_cpp_mvp5_irc
 */
#include "irc_server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace logcrafter::cpp {
namespace {

constexpr int kMaxLine = 512;
constexpr int kSelectTimeoutMs = 250;

void send_all(int fd, const std::string &line) {
    const char *data = line.c_str();
    std::size_t remaining = line.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(fd, data, remaining, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

std::tm safe_localtime(std::time_t timestamp) {
    std::tm result{};
#if defined(_POSIX_VERSION)
    localtime_r(&timestamp, &result);
#else
    std::tm *tmp = std::localtime(&timestamp);
    if (tmp != nullptr) {
        result = *tmp;
    }
#endif
    return result;
}

std::vector<std::string> split_channels(const std::vector<std::string> &params) {
    std::vector<std::string> channels;
    if (params.empty()) {
        return channels;
    }
    std::stringstream ss(params.front());
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            channels.push_back(token);
        }
    }
    return channels;
}

} // namespace

IRCServer::IRCServer()
    : server_name_("logcrafter"),
      listen_fd_(-1),
      running_(false),
      worker_(),
      mutex_(),
      clients_(),
      channel_manager_(),
      active_clients_(0) {}

IRCServer::~IRCServer() { shutdown(); }

int IRCServer::start(int port) {
    shutdown();

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::perror("irc socket");
        return -1;
    }

    const int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::perror("irc bind");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return -1;
    }

    if (::listen(listen_fd_, 32) < 0) {
        std::perror("irc listen");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return -1;
    }

    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&IRCServer::run_loop, this);
    std::cerr << "[lc][info] IRC server listening on port " << port << std::endl;
    return 0;
}

void IRCServer::request_stop() {
    running_.store(false, std::memory_order_release);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
    }
}

void IRCServer::shutdown() {
    request_stop();
    if (worker_.joinable()) {
        worker_.join();
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &entry : clients_) {
        ::close(entry.first);
    }
    clients_.clear();
    channel_manager_.reset();
    active_clients_.store(0, std::memory_order_relaxed);
}

void IRCServer::publish_log(const std::string &message, std::time_t timestamp) {
    std::vector<PendingSend> sends;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto deliveries = channel_manager_.prepare_log_deliveries(message);
        for (const auto &delivery : deliveries) {
            auto it = clients_.find(delivery.client_fd);
            if (it == clients_.end()) {
                continue;
            }
            const IRCClient &client = it->second;
            if (!client.registered) {
                continue;
            }
            sends.push_back({delivery.client_fd,
                             format_privmsg(server_name_, delivery.channel, message, timestamp)});
        }
    }
    send_lines(sends);
}

std::size_t IRCServer::active_clients() const { return active_clients_.load(std::memory_order_relaxed); }

void IRCServer::run_loop() {
    while (running_.load(std::memory_order_acquire)) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = -1;
        std::vector<int> client_fds;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (listen_fd_ >= 0) {
                FD_SET(listen_fd_, &read_fds);
                max_fd = listen_fd_;
            }
            for (const auto &entry : clients_) {
                FD_SET(entry.first, &read_fds);
                if (entry.first > max_fd) {
                    max_fd = entry.first;
                }
                client_fds.push_back(entry.first);
            }
        }

        struct timeval timeout;
        timeout.tv_sec = kSelectTimeoutMs / 1000;
        timeout.tv_usec = (kSelectTimeoutMs % 1000) * 1000;

        const int ready = ::select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("irc select");
            break;
        }

        if (listen_fd_ >= 0 && FD_ISSET(listen_fd_, &read_fds)) {
            handle_accept();
        }

        for (int fd : client_fds) {
            if (FD_ISSET(fd, &read_fds)) {
                if (!handle_client_input(fd)) {
                    continue;
                }
            }
        }
    }
}

void IRCServer::handle_accept() {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));

    const int client_fd = ::accept(listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
    if (client_fd < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            std::perror("irc accept");
        }
        return;
    }

    set_socket_nonblocking(client_fd);

    IRCClient client{};
    client.fd = client_fd;
    client.has_nick = false;
    client.has_user = false;
    client.registered = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.emplace(client_fd, std::move(client));
        active_clients_.fetch_add(1, std::memory_order_relaxed);
    }

    PendingSend welcome{client_fd,
                        ":" + server_name_ + " NOTICE * :LogCrafter IRC ready. Send NICK and USER to begin.\r\n"};
    send_lines({welcome});
}

bool IRCServer::handle_client_input(int client_fd) {
    char buffer[kMaxLine];
    const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
        if (received == 0 || (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)) {
            std::lock_guard<std::mutex> lock(mutex_);
            close_client_locked(client_fd);
        }
        return false;
    }

    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(client_fd);
        if (it == clients_.end()) {
            return false;
        }
        IRCClient &client = it->second;
        client.recv_buffer.append(buffer, static_cast<std::size_t>(received));
        lines = drain_client_lines(client);
    }

    bool should_close = false;
    for (const std::string &line : lines) {
        IRCCommand command;
        if (!IRCCommandParser::parse(line, command)) {
            continue;
        }
        bool client_closed = false;
        const auto sends = process_command(client_fd, command, client_closed);
        send_lines(sends);
        if (client_closed) {
            should_close = true;
            break;
        }
    }

    if (should_close) {
        std::lock_guard<std::mutex> lock(mutex_);
        close_client_locked(client_fd);
        return false;
    }

    return true;
}

std::vector<std::string> IRCServer::drain_client_lines(IRCClient &client) {
    std::vector<std::string> lines;
    while (true) {
        std::size_t newline = client.recv_buffer.find('\n');
        if (newline == std::string::npos) {
            break;
        }
        std::string raw = client.recv_buffer.substr(0, newline);
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        lines.push_back(raw);
        client.recv_buffer.erase(0, newline + 1);
    }
    if (client.recv_buffer.size() > kMaxLine) {
        client.recv_buffer.erase(0, client.recv_buffer.size() - kMaxLine);
    }
    return lines;
}

std::vector<IRCServer::PendingSend> IRCServer::process_command(int client_fd, const IRCCommand &command,
                                                               bool &client_closed) {
    std::vector<PendingSend> sends;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        client_closed = true;
        return sends;
    }

    IRCClient &client = it->second;
    client_closed = false;

    const std::string &verb = command.verb;
    if (verb == "NICK") {
        if (command.params.empty() || command.params.front().empty()) {
            sends.push_back(make_notice(client, "Nickname required"));
            return sends;
        }
        client.nickname = command.params.front();
        client.has_nick = true;
        if (client.has_user && !client.registered) {
            auto welcome = register_client(client);
            sends.insert(sends.end(), welcome.begin(), welcome.end());
        }
        return sends;
    }

    if (verb == "USER") {
        if (command.params.size() < 4) {
            sends.push_back(make_notice(client, "USER requires 4 parameters"));
            return sends;
        }
        client.username = command.params[0];
        client.has_user = true;
        if (client.has_nick && !client.registered) {
            auto welcome = register_client(client);
            sends.insert(sends.end(), welcome.begin(), welcome.end());
        }
        return sends;
    }

    if (!client.registered) {
        if (verb == "PING") {
            if (!command.params.empty()) {
                sends.push_back({client.fd, ":" + server_name_ + " PONG " + server_name_ + " :" + command.params.front() + "\r\n"});
            }
            return sends;
        }
        if (verb == "QUIT") {
            sends.push_back({client.fd, ":" + server_name_ + " ERROR :Closing link\r\n"});
            client_closed = true;
            return sends;
        }
        sends.push_back(make_notice(client, "Register first using NICK and USER"));
        return sends;
    }

    if (verb == "PING") {
        if (!command.params.empty()) {
            sends.push_back({client.fd, ":" + server_name_ + " PONG " + server_name_ + " :" + command.params.front() + "\r\n"});
        }
        return sends;
    }

    if (verb == "JOIN") {
        auto join_lines = handle_join(client, command);
        sends.insert(sends.end(), join_lines.begin(), join_lines.end());
        return sends;
    }

    if (verb == "PART") {
        auto part_lines = handle_part(client, command);
        sends.insert(sends.end(), part_lines.begin(), part_lines.end());
        return sends;
    }

    if (verb == "QUIT") {
        sends.push_back({client.fd, ":" + client.nickname + " QUIT :Goodbye\r\n"});
        client_closed = true;
        return sends;
    }

    if (verb == "PRIVMSG") {
        sends.push_back(make_notice(client, "Server commands are not available in MVP5."));
        return sends;
    }

    sends.push_back(make_unknown_command(client, verb));
    return sends;
}

std::vector<IRCServer::PendingSend> IRCServer::register_client(IRCClient &client) {
    client.registered = true;
    std::vector<PendingSend> sends;
    sends.push_back({client.fd, ":" + server_name_ + " 001 " + client.nickname + " :Welcome to LogCrafter IRC\r\n"});
    sends.push_back({client.fd, ":" + server_name_ + " 422 " + client.nickname + " :No MOTD available\r\n"});
    const std::vector<std::string> auto_channels = {"#logs-all"};
    const auto joined = channel_manager_.join_channels(client.fd, auto_channels);
    for (const std::string &name : joined) {
        sends.push_back({client.fd, ":" + client.nickname + " JOIN :" + name + "\r\n"});
        sends.push_back({client.fd, ":" + server_name_ + " 332 " + client.nickname + " " + name + " :Log stream channel\r\n"});
    }
    sends.push_back(make_notice(client, "Join #logs-error, #logs-warning, #logs-info, or #logs-debug for filtered streams."));
    return sends;
}

std::vector<IRCServer::PendingSend> IRCServer::handle_join(IRCClient &client, const IRCCommand &command) {
    std::vector<PendingSend> sends;
    const auto requested = split_channels(command.params);
    if (requested.empty()) {
        sends.push_back(make_notice(client, "JOIN requires a channel name."));
        return sends;
    }
    const auto joined = channel_manager_.join_channels(client.fd, requested);
    if (joined.empty()) {
        sends.push_back(make_notice(client, "No channels were joined."));
        return sends;
    }
    for (const std::string &name : joined) {
        sends.push_back({client.fd, ":" + client.nickname + " JOIN :" + name + "\r\n"});
        sends.push_back({client.fd, ":" + server_name_ + " 332 " + client.nickname + " " + name + " :Log stream channel\r\n"});
    }
    return sends;
}

std::vector<IRCServer::PendingSend> IRCServer::handle_part(IRCClient &client, const IRCCommand &command) {
    std::vector<PendingSend> sends;
    const auto requested = split_channels(command.params);
    if (requested.empty()) {
        sends.push_back(make_notice(client, "PART requires a channel name."));
        return sends;
    }
    const auto parted = channel_manager_.part_channels(client.fd, requested);
    if (parted.empty()) {
        sends.push_back(make_notice(client, "No channels were parted."));
        return sends;
    }
    for (const std::string &name : parted) {
        sends.push_back({client.fd, ":" + client.nickname + " PART " + name + "\r\n"});
    }
    return sends;
}

IRCServer::PendingSend IRCServer::make_notice(const IRCClient &client, const std::string &message) const {
    const std::string target = client.registered ? client.nickname : std::string("*");
    return {client.fd, ":" + server_name_ + " NOTICE " + target + " :" + message + "\r\n"};
}

IRCServer::PendingSend IRCServer::make_unknown_command(const IRCClient &client, const std::string &command) const {
    return {client.fd, ":" + server_name_ + " 421 " + client.nickname + " " + command + " :Unknown command\r\n"};
}

void IRCServer::close_client_locked(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }
    channel_manager_.remove_client(client_fd);
    ::close(client_fd);
    clients_.erase(it);
    active_clients_.fetch_sub(1, std::memory_order_relaxed);
}

void IRCServer::send_lines(const std::vector<PendingSend> &sends) {
    for (const auto &send : sends) {
        send_all(send.fd, send.line);
    }
}

void IRCServer::set_socket_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return;
    }
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::string IRCServer::format_privmsg(const std::string &server_name, const std::string &channel,
                                      const std::string &message, std::time_t timestamp) {
    std::ostringstream oss;
    const std::tm tm_value = safe_localtime(timestamp);
    oss << ':' << server_name << " PRIVMSG " << channel << " :";
    oss << '[' << std::put_time(&tm_value, "%Y-%m-%d %H:%M:%S") << "] " << message << "\r\n";
    return oss.str();
}

} // namespace logcrafter::cpp
