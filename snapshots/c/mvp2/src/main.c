/*
 * Sequence: SEQ0010
 * Track: C
 * MVP: mvp2
 * Change: Keep the CLI entry point aligned with the MVP2 threaded server runtime and signal handling.
 * Tests: smoke_thread_pool_query
 */
#include "lc_server.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static LCServer g_server;

static void lc_sig_handler(int signal_number) {
    (void)signal_number;
    lc_server_request_stop(&g_server);
}

static int lc_parse_port(const char *value, int fallback) {
    if (value == NULL) {
        return fallback;
    }
    char *endptr = NULL;
    errno = 0;
    long parsed = strtol(value, &endptr, 10);
    if (errno != 0 || endptr == value || parsed <= 0 || parsed > 65535) {
        return fallback;
    }
    return (int)parsed;
}

static void lc_print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [-p PORT] [-h]\n"
            "  -p PORT  Set log listener port (query uses 9998)\n"
            "  -h       Show this help text\n",
            prog);
}

int main(int argc, char **argv) {
    LCServerConfig config = lc_server_config_default();

    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1) {
        switch (opt) {
        case 'p':
            config.log_port = lc_parse_port(optarg, config.log_port);
            break;
        case 'h':
            lc_print_usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            lc_print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind != argc) {
        lc_print_usage(argv[0]);
        return EXIT_FAILURE;
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
