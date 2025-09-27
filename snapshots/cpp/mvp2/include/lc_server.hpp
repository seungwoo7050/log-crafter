/*
 * Sequence: SEQ0034
 * Track: C++
 * MVP: mvp2
 * Change: Extend the C++ server interface with worker-pool configuration and shared log buffer accessors.
 * Tests: smoke_cpp_mvp2_thread_pool
 */
#ifndef LOGCRAFTER_CPP_LC_SERVER_HPP
#define LOGCRAFTER_CPP_LC_SERVER_HPP

#include <atomic>
#include <cstddef>
#include <string>

#include "log_buffer.hpp"
#include "thread_pool.hpp"

namespace logcrafter::cpp {

struct ServerConfig {
    int log_port;
    int query_port;
    int max_pending_connections;
    int select_timeout_ms;
    std::size_t buffer_capacity;
    int worker_threads;
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

    static constexpr std::size_t kMaxLogLength = 1024;
    static constexpr std::size_t kDefaultLogCapacity = 10000;
    static constexpr int kDefaultWorkerThreads = 4;

private:
    int create_listener(int port, int backlog);
    void dispatch_log_client(int client_fd);
    void dispatch_query_client(int client_fd);
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

    ThreadPool thread_pool_;
    LogBuffer log_buffer_;
    std::atomic<int> active_log_clients_;
    std::atomic<int> active_query_clients_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_LC_SERVER_HPP
