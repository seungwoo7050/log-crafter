#include "server.h"
#include "query_handler.h"

// [SEQUENCE: 4] Global server instance for signal handling
static log_server_t* g_server = NULL;

// [SEQUENCE: 5] Signal handler for graceful shutdown
static void sigint_handler(int sig) {
    (void)sig;
    if (g_server) {
        g_server->running = 0;
    }
}

// [SEQUENCE: 132] Thread pool job function for client handling
static void handle_client_job(void* arg) {
    client_job_t* job = (client_job_t*)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    // [SEQUENCE: 133] Read data from client in thread
    while ((bytes_read = recv(job->client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Remove trailing newline if present
        if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }
        
        // [SEQUENCE: 134] Store log in buffer instead of printing
        log_buffer_push(job->server->log_buffer, buffer);
    }
    
    // [SEQUENCE: 135] Client disconnected
    if (bytes_read == 0) {
        printf("Client disconnected (fd=%d)\n", job->client_fd);
    } else if (bytes_read < 0) {
        perror("recv");
    }
    
    // [SEQUENCE: 136] Cleanup client connection
    close(job->client_fd);
    
    // Update server state (thread-safe would need mutex)
    printf("Client handler completed for %s:%d\n",
           inet_ntoa(job->client_addr.sin_addr), 
           ntohs(job->client_addr.sin_port));
    
    free(job);
}

// [SEQUENCE: 6] Server creation and initialization - MVP2 version
log_server_t* server_create(int port) {
    log_server_t* server = malloc(sizeof(log_server_t));
    if (!server) {
        perror("malloc");
        return NULL;
    }
    
    server->port = port;
    server->query_port = QUERY_PORT;
    server->listen_fd = -1;
    server->query_fd = -1;
    server->max_fd = -1;
    server->client_count = 0;
    server->running = 1;
    FD_ZERO(&server->master_set);
    FD_ZERO(&server->read_set);
    
    // [SEQUENCE: 137] Initialize thread pool and log buffer
    server->thread_pool = thread_pool_create(DEFAULT_THREAD_COUNT);
    if (!server->thread_pool) {
        free(server);
        return NULL;
    }
    
    server->log_buffer = log_buffer_create(DEFAULT_BUFFER_SIZE);
    if (!server->log_buffer) {
        thread_pool_destroy(server->thread_pool);
        free(server);
        return NULL;
    }
    
    return server;
}

void server_destroy(log_server_t* server) {
    if (!server) return;
    
    // [SEQUENCE: 138] Cleanup thread pool and log buffer first
    if (server->thread_pool) {
        thread_pool_destroy(server->thread_pool);
    }
    
    if (server->log_buffer) {
        log_buffer_destroy(server->log_buffer);
    }
    
    // Close all connections
    for (int fd = 0; fd <= server->max_fd; fd++) {
        if (FD_ISSET(fd, &server->master_set)) {
            close(fd);
        }
    }
    
    free(server);
}

// [SEQUENCE: 139] Initialize both log and query listeners
static int init_listener(int port, const char* name) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }
    
    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }
    
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }
    
    printf("%s listener on port %d\n", name, port);
    return listen_fd;
}

int server_init(log_server_t* server) {
    // [SEQUENCE: 140] Initialize log listener
    server->listen_fd = init_listener(server->port, "Log");
    if (server->listen_fd < 0) {
        return -1;
    }
    
    // [SEQUENCE: 141] Initialize query listener
    server->query_fd = init_listener(server->query_port, "Query");
    if (server->query_fd < 0) {
        close(server->listen_fd);
        return -1;
    }
    
    // [SEQUENCE: 142] Add both listeners to master set
    FD_SET(server->listen_fd, &server->master_set);
    FD_SET(server->query_fd, &server->master_set);
    server->max_fd = (server->listen_fd > server->query_fd) ? 
                     server->listen_fd : server->query_fd;
    
    // Setup signal handler
    g_server = server;
    signal(SIGINT, sigint_handler);
    
    printf("LogCrafter-C MVP2 server ready (Thread Pool: %d threads)\n", 
           DEFAULT_THREAD_COUNT);
    return 0;
}

void server_run(log_server_t* server) {
    struct timeval tv;
    int select_ret;
    
    while (server->running) {
        server->read_set = server->master_set;
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        select_ret = select(server->max_fd + 1, &server->read_set, NULL, NULL, &tv);
        
        if (select_ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        
        if (select_ret == 0) continue;
        
        // [SEQUENCE: 143] Check for new connections
        if (FD_ISSET(server->listen_fd, &server->read_set)) {
            handle_new_connection(server);
        }
        
        // [SEQUENCE: 144] Check for query connections
        if (FD_ISSET(server->query_fd, &server->read_set)) {
            handle_query_connection(server);
        }
    }
    
    printf("\nShutting down server...\n");
    
    // [SEQUENCE: 145] Wait for thread pool to finish
    thread_pool_wait(server->thread_pool);
}

void server_stop(log_server_t* server) {
    server->running = 0;
}

void handle_new_connection(log_server_t* server) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int new_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (new_fd < 0) {
        perror("accept");
        return;
    }
    
    if (server->client_count >= MAX_CLIENTS) {
        printf("Maximum clients reached. Rejecting connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(new_fd);
        return;
    }
    
    // [SEQUENCE: 146] Create job for thread pool
    client_job_t* job = malloc(sizeof(client_job_t));
    if (!job) {
        perror("malloc");
        close(new_fd);
        return;
    }
    
    job->client_fd = new_fd;
    job->server = server;
    job->client_addr = client_addr;
    
    // [SEQUENCE: 147] Submit job to thread pool
    if (thread_pool_add_job(server->thread_pool, handle_client_job, job) != 0) {
        printf("Failed to add job to thread pool\n");
        free(job);
        close(new_fd);
        return;
    }
    
    server->client_count++;
    
    printf("New connection from %s:%d delegated to thread pool (total clients=%d)\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
           server->client_count);
}

// [SEQUENCE: 148] MVP1 functions no longer needed
void handle_client_data(log_server_t* server, int client_fd) {
    (void)server;
    (void)client_fd;
    // Not used in MVP2 - handled by thread pool
}

void close_client_connection(log_server_t* server, int client_fd) {
    (void)server;
    (void)client_fd;
    // Not used in MVP2 - handled by thread pool
}
