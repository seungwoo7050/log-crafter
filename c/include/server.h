#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <signal.h>
#include "thread_pool.h"
#include "log_buffer.h"
#include "persistence.h"

#define DEFAULT_PORT 9999
#define QUERY_PORT 9998
#define MAX_CLIENTS 1024
#define BUFFER_SIZE 4096
#define BACKLOG 10

// [SEQUENCE: 1] Server structure definition - MVP2 with thread pool
typedef struct {
    int port;
    int query_port;
    int listen_fd;
    int query_fd;
    fd_set master_set;
    fd_set read_set;
    int max_fd;
    int client_count;
    volatile sig_atomic_t running;
    
    // [SEQUENCE: 120] MVP2 additions
    thread_pool_t* thread_pool;
    log_buffer_t* log_buffer;
    
    // [SEQUENCE: 348] MVP4 persistence
    persistence_manager_t* persistence;
    
    // [SEQUENCE: 400] MVP5 thread safety
    pthread_mutex_t client_count_mutex;
} log_server_t;

// [SEQUENCE: 121] Client job context for thread pool
typedef struct {
    int client_fd;
    log_server_t* server;
    struct sockaddr_in client_addr;
} client_job_t;

// [SEQUENCE: 2] Server lifecycle functions
log_server_t* server_create(int port);
void server_destroy(log_server_t* server);
int server_init(log_server_t* server);
void server_run(log_server_t* server);
void server_stop(log_server_t* server);

// [SEQUENCE: 3] Connection handling functions
void handle_new_connection(log_server_t* server);
void handle_client_data(log_server_t* server, int client_fd);
void close_client_connection(log_server_t* server, int client_fd);

#endif // SERVER_H
