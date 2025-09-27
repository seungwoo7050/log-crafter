/*
 * Sequence: SEQ0031
 * Track: C++
 * MVP: mvp1
 * Change: Expand the C++ server interface with in-memory log statistics for MVP1 query support.
 * Tests: smoke_cpp_mvp1_query
 */
#ifndef LOGCRAFTER_CPP_LC_SERVER_HPP
#define LOGCRAFTER_CPP_LC_SERVER_HPP

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace logcrafter::cpp {

struct ServerConfig {
    int log_port;
    int query_port;
    int max_pending_connections;
    int select_timeout_ms;
};

ServerConfig default_config();

class Server {
public:
    Server();

    int init(const ServerConfig &config);
    int run();
    void request_stop();
    void shutdown();

    const ServerConfig &config() const { return config_; }

private:
    static constexpr std::size_t kMaxLogLength = 1024;
    static constexpr std::size_t kDefaultLogCapacity = 10000;

    int create_listener(int port, int backlog);
    void handle_log_client(int client_fd);
    void handle_query_client(int client_fd);
    void store_log(const std::string &message);
    void send_help(int client_fd) const;
    void send_count(int client_fd) const;
    void send_stats(int client_fd) const;
    void send_query_keyword(int client_fd, const std::string &keyword) const;

    ServerConfig config_;
    int log_listener_fd_;
    int query_listener_fd_;
    std::atomic<bool> running_;

    std::vector<std::string> logs_;
    std::size_t log_capacity_;
    std::size_t log_size_;
    std::size_t log_head_;
    unsigned long total_logs_;
    unsigned long dropped_logs_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_LC_SERVER_HPP
