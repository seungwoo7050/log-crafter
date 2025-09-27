/*
 * Sequence: SEQ0033
 * Track: C++
 * MVP: mvp1
 * Change: Refresh the MVP1 CLI entry point and signal wiring for the C++ log/query server.
 * Tests: smoke_cpp_mvp1_query
 */
#include "lc_server.hpp"

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <signal.h>

namespace {

logcrafter::cpp::Server g_server;

void handle_signal(int) {
    g_server.request_stop();
}

int parse_int(const char *value, int fallback) {
    if (value == nullptr) {
        return fallback;
    }
    char *endptr = nullptr;
    const long parsed = std::strtol(value, &endptr, 10);
    if (endptr == value || parsed <= 0 || parsed > 65535) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

void print_usage(const char *prog) {
    std::cerr << "Usage: " << prog << " [--log-port PORT] [--query-port PORT]" << std::endl
              << "Runs the LogCrafter C++ MVP1 server with in-memory query support." << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    logcrafter::cpp::ServerConfig config = logcrafter::cpp::default_config();

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--log-port") == 0 && i + 1 < argc) {
            config.log_port = parse_int(argv[++i], config.log_port);
        } else if (std::strcmp(argv[i], "--query-port") == 0 && i + 1 < argc) {
            config.query_port = parse_int(argv[++i], config.query_port);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (g_server.init(config) != 0) {
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    const int result = g_server.run();
    g_server.shutdown();
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
