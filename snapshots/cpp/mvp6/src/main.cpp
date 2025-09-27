/*
 * Sequence: SEQ0075
 * Track: C++
 * MVP: mvp6
 * Change: Expand CLI parsing for IRC server naming, auto-join channels, and signal handling improvements.
 * Tests: smoke_cpp_mvp6_irc
 */
#include "lc_server.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <signal.h>

namespace {

logcrafter::cpp::Server g_server;

void handle_signal(int) {
    g_server.request_stop();
}

int parse_port(const char *value, int fallback) {
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

std::size_t parse_capacity(const char *value, std::size_t fallback) {
    if (value == nullptr) {
        return fallback;
    }
    char *endptr = nullptr;
    const unsigned long long parsed = std::strtoull(value, &endptr, 10);
    if (endptr == value || *endptr != '\0') {
        return fallback;
    }
    if (parsed == 0ULL || parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        return fallback;
    }
    return static_cast<std::size_t>(parsed);
}

int parse_workers(const char *value, int fallback) {
    if (value == nullptr) {
        return fallback;
    }
    char *endptr = nullptr;
    const long parsed = std::strtol(value, &endptr, 10);
    if (endptr == value || parsed < 1 || parsed > 256) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool parse_size_mb(const char *value, std::size_t &result) {
    if (value == nullptr) {
        return false;
    }
    char *endptr = nullptr;
    const unsigned long long parsed = std::strtoull(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' || parsed == 0ULL) {
        return false;
    }
    const unsigned long long multiplier = 1024ULL * 1024ULL;
    if (parsed > std::numeric_limits<std::size_t>::max() / multiplier) {
        return false;
    }
    result = static_cast<std::size_t>(parsed * multiplier);
    return true;
}

bool parse_positive_size(const char *value, std::size_t &result, std::size_t minimum, std::size_t maximum) {
    if (value == nullptr) {
        return false;
    }
    char *endptr = nullptr;
    const unsigned long long parsed = std::strtoull(value, &endptr, 10);
    if (endptr == value || *endptr != '\0') {
        return false;
    }
    if (parsed < minimum || parsed > maximum) {
        return false;
    }
    result = static_cast<std::size_t>(parsed);
    return true;
}

std::vector<std::string> parse_channel_list(const char *value) {
    std::vector<std::string> channels;
    if (value == nullptr) {
        return channels;
    }
    std::string current;
    for (const char *ptr = value; *ptr != '\0'; ++ptr) {
        if (*ptr == ',') {
            if (!current.empty()) {
                channels.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(*ptr);
        }
    }
    if (!current.empty()) {
        channels.push_back(current);
    }
    return channels;
}

void print_usage(const char *prog) {
    std::cerr << "Usage: " << prog
              << " [--log-port PORT] [--query-port PORT] [--capacity N] [--workers N]" << std::endl
              << "       [--enable-persistence|--disable-persistence]" << std::endl
              << "       [--persistence-dir PATH] [--persistence-max-size MB]" << std::endl
              << "       [--persistence-max-files N]" << std::endl
              << "       [--enable-irc|--disable-irc] [--irc-port PORT]" << std::endl
              << "       [--irc-server-name NAME] [--irc-auto-join chan1,chan2]" << std::endl
              << "Runs the LogCrafter C++ MVP6 server with persistence, IRC streaming, and advanced query handling." << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    logcrafter::cpp::ServerConfig config = logcrafter::cpp::default_config();

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--log-port") == 0 && i + 1 < argc) {
            config.log_port = parse_port(argv[++i], config.log_port);
        } else if (std::strcmp(argv[i], "--query-port") == 0 && i + 1 < argc) {
            config.query_port = parse_port(argv[++i], config.query_port);
        } else if (std::strcmp(argv[i], "--capacity") == 0 && i + 1 < argc) {
            config.buffer_capacity = parse_capacity(argv[++i], config.buffer_capacity);
        } else if (std::strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            config.worker_threads = parse_workers(argv[++i], config.worker_threads);
        } else if (std::strcmp(argv[i], "--enable-persistence") == 0) {
            config.persistence_enabled = true;
        } else if (std::strcmp(argv[i], "--persistence-dir") == 0 && i + 1 < argc) {
            const char *value = argv[++i];
            if (value == nullptr || *value == '\0') {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            config.persistence_directory = value;
            config.persistence_enabled = true;
        } else if (std::strcmp(argv[i], "--persistence-max-size") == 0 && i + 1 < argc) {
            std::size_t bytes = 0;
            if (!parse_size_mb(argv[++i], bytes)) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            config.persistence_max_file_size = bytes;
            config.persistence_enabled = true;
        } else if (std::strcmp(argv[i], "--persistence-max-files") == 0 && i + 1 < argc) {
            std::size_t max_files = 0;
            if (!parse_positive_size(argv[++i], max_files, 1, 1000)) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            config.persistence_max_files = max_files;
            config.persistence_enabled = true;
        } else if (std::strcmp(argv[i], "--disable-persistence") == 0) {
            config.persistence_enabled = false;
        } else if (std::strcmp(argv[i], "--enable-irc") == 0) {
            config.irc_enabled = true;
        } else if (std::strcmp(argv[i], "--irc-port") == 0 && i + 1 < argc) {
            config.irc_port = parse_port(argv[++i], config.irc_port);
            config.irc_enabled = true;
        } else if (std::strcmp(argv[i], "--irc-server-name") == 0 && i + 1 < argc) {
            const char *value = argv[++i];
            if (value == nullptr || *value == '\0') {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            config.irc_server_name = value;
            config.irc_enabled = true;
        } else if (std::strcmp(argv[i], "--irc-auto-join") == 0 && i + 1 < argc) {
            const auto channels = parse_channel_list(argv[++i]);
            if (channels.empty()) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            config.irc_auto_join = channels;
            config.irc_enabled = true;
        } else if (std::strcmp(argv[i], "--disable-irc") == 0) {
            config.irc_enabled = false;
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
    std::signal(SIGPIPE, SIG_IGN);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    const int result = g_server.run();
    g_server.shutdown();
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
