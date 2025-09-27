/*
 * Sequence: SEQ0022
 * Track: C
 * MVP: mvp4
 * Change: Extend the CLI with persistence toggles and routing to the asynchronous writer before launching the server.
 * Tests: smoke_persistence_rotation
 */
#include "lc_server.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
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

static int lc_parse_size_mb(const char *value, size_t *result) {
    if (value == NULL || result == NULL) {
        return -1;
    }
    char *endptr = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(value, &endptr, 10);
    if (errno != 0 || endptr == value || *endptr != '\0' || parsed == 0ULL) {
        return -1;
    }
    const unsigned long long multiplier = 1024ULL * 1024ULL;
    if (parsed > (unsigned long long)(SIZE_MAX / multiplier)) {
        return -1;
    }
    *result = (size_t)(parsed * multiplier);
    return 0;
}

static void lc_print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [-p PORT] [-P] [-d DIR] [-s SIZE_MB] [-h]\n"
            "  -p PORT    Set log listener port (query uses 9998)\n"
            "  -P         Enable persistence (writes to disk)\n"
            "  -d DIR     Set persistence directory (implies -P)\n"
            "  -s SIZE_MB Set max persistence file size in MB (implies -P)\n"
            "  -h         Show this help text\n",
            prog);
}

int main(int argc, char **argv) {
    LCServerConfig config = lc_server_config_default();

    int opt;
    while ((opt = getopt(argc, argv, "hp:Ps:d:")) != -1) {
        switch (opt) {
        case 'p':
            config.log_port = lc_parse_port(optarg, config.log_port);
            break;
        case 'P':
            config.persistence_enabled = 1;
            break;
        case 'd': {
            if (optarg == NULL || optarg[0] == '\0') {
                fprintf(stderr, "Invalid persistence directory.\n");
                return EXIT_FAILURE;
            }
            size_t length = strlen(optarg);
            if (length >= sizeof(config.persistence_directory)) {
                fprintf(stderr, "Persistence directory path is too long.\n");
                return EXIT_FAILURE;
            }
            memcpy(config.persistence_directory, optarg, length + 1);
            config.persistence_enabled = 1;
            break;
        }
        case 's': {
            size_t bytes = 0;
            if (lc_parse_size_mb(optarg, &bytes) != 0) {
                fprintf(stderr, "Invalid value for -s. Provide a positive integer in megabytes.\n");
                return EXIT_FAILURE;
            }
            config.persistence_max_file_size = bytes;
            config.persistence_enabled = 1;
            break;
        }
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
