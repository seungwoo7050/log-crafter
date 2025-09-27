#include "lc_server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LCServer g_server;

static void lc_sig_handler(int signal_number) {
    (void)signal_number;
    lc_server_request_stop(&g_server);
}

static int lc_parse_int(const char *value, int fallback) {
    if (value == NULL) {
        return fallback;
    }
    char *endptr = NULL;
    long parsed = strtol(value, &endptr, 10);
    if (endptr == value || parsed <= 0 || parsed > 65535) {
        return fallback;
    }
    return (int)parsed;
}

static void lc_print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--log-port PORT] [--query-port PORT]\n"
            "Runs the LogCrafter MVP0 bootstrap server.\n",
            prog);
}

int main(int argc, char **argv) {
    LCServerConfig config = lc_server_config_default();

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--log-port") == 0 && i + 1 < argc) {
            config.log_port = lc_parse_int(argv[++i], config.log_port);
        } else if (strcmp(argv[i], "--query-port") == 0 && i + 1 < argc) {
            config.query_port = lc_parse_int(argv[++i], config.query_port);
        } else if (strcmp(argv[i], "--help") == 0) {
            lc_print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            lc_print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (lc_server_init(&g_server, &config) != 0) {
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = lc_sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int result = lc_server_run(&g_server);
    lc_server_shutdown(&g_server);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
