/*
 * Sequence: SEQ0030
 * Track: C++
 * MVP: mvp0
 * Change: Introduce the MVP0 bootstrap server skeleton with configuration helpers and loop control.
 * Tests: smoke_cpp_mvp0_bind_accept
 */
#ifndef LOGCRAFTER_CPP_LC_SERVER_HPP
#define LOGCRAFTER_CPP_LC_SERVER_HPP

#include <atomic>

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
    int create_listener(int port, int backlog);
    void handle_log_client(int client_fd);
    void handle_query_client(int client_fd);

    ServerConfig config_;
    int log_listener_fd_;
    int query_listener_fd_;
    std::atomic<bool> running_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_LC_SERVER_HPP
