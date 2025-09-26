#include "server.h"
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

// [SEQUENCE: 28] Main entry point
int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    log_server_t* server = NULL;
    
    // [SEQUENCE: 349] MVP4 persistence options
    bool persistence_enabled = false;
    char log_directory[256] = "./logs";
    size_t max_file_size = 10 * 1024 * 1024; // 10MB default
    
    // [SEQUENCE: 29] Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:d:s:Ph")) != -1) {
        switch (opt) {
            case 'p': {
                // [SEQUENCE: 408] Safe integer parsing with overflow check
                char* endptr;
                errno = 0;
                long value = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || value <= 0 || value > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                port = (int)value;
                break;
            }
            case 'P':
                persistence_enabled = true;
                break;
            case 'd':
                strncpy(log_directory, optarg, sizeof(log_directory) - 1);
                break;
            case 's': {
                // [SEQUENCE: 409] Prevent integer overflow in file size calculation
                char* endptr;
                errno = 0;
                long mb_value = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || mb_value <= 0) {
                    fprintf(stderr, "Invalid file size: %s\n", optarg);
                    return 1;
                }
                // Check for overflow before multiplication
                if (mb_value > (long)(SIZE_MAX / (1024 * 1024))) {
                    fprintf(stderr, "File size too large: %s MB\n", optarg);
                    return 1;
                }
                max_file_size = (size_t)mb_value * 1024 * 1024;
                break;
            }
            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("Options:\n");
                printf("  -p PORT    Set server port (default: %d)\n", DEFAULT_PORT);
                printf("  -P         Enable persistence to disk\n");
                printf("  -d DIR     Set log directory (default: ./logs)\n");
                printf("  -s SIZE    Set max file size in MB (default: 10)\n");
                printf("  -h         Show this help message\n");
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-p port] [-P] [-d dir] [-s size] [-h]\n", argv[0]);
                return 1;
        }
    }
    
    // [SEQUENCE: 30] Create and initialize server
    server = server_create(port);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    // [SEQUENCE: 350] Configure persistence if enabled
    if (persistence_enabled) {
        persistence_config_t persist_config = {
            .enabled = true,
            .max_file_size = max_file_size,
            .max_files = 10,
            .flush_interval_ms = 1000,
            .load_on_startup = true
        };
        strncpy(persist_config.log_directory, log_directory, sizeof(persist_config.log_directory) - 1);
        
        server->persistence = persistence_create(&persist_config);
        if (!server->persistence) {
            fprintf(stderr, "Failed to initialize persistence\n");
            server_destroy(server);
            return 1;
        }
        
        printf("Persistence enabled: directory=%s, max_file_size=%zuMB\n", 
               log_directory, max_file_size / (1024 * 1024));
    }
    
    if (server_init(server) < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        server_destroy(server);
        return 1;
    }
    
    // [SEQUENCE: 31] Run server
    printf("LogCrafter-C server starting on port %d\n", port);
    if (persistence_enabled) {
        printf("Persistence: ENABLED (logs saved to %s)\n", log_directory);
    } else {
        printf("Persistence: DISABLED (logs in memory only)\n");
    }
    printf("Press Ctrl+C to stop\n\n");
    
    server_run(server);
    
    // [SEQUENCE: 32] Cleanup
    server_destroy(server);
    return 0;
}
