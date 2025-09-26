#include "server.h"

// [SEQUENCE: 28] Main entry point
int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    log_server_t* server = NULL;
    
    // [SEQUENCE: 29] Parse command line arguments
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            return 1;
        }
    }
    
    // [SEQUENCE: 30] Create and initialize server
    server = server_create(port);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    if (server_init(server) < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        server_destroy(server);
        return 1;
    }
    
    // [SEQUENCE: 31] Run server
    printf("LogCrafter-C server starting on port %d\n", port);
    printf("Press Ctrl+C to stop\n\n");
    
    server_run(server);
    
    // [SEQUENCE: 32] Cleanup
    server_destroy(server);
    return 0;
}
