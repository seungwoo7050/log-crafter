#include "query_handler.h"
#include "query_parser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// [SEQUENCE: 124] Handle new query connection
void handle_query_connection(log_server_t* server) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server->query_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept query");
        return;
    }
    
    printf("Query connection from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    // [SEQUENCE: 125] Read query command
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        // Remove trailing newline
        if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }
        
        process_query_command(server, client_fd, buffer);
    }
    
    close(client_fd);
}

// [SEQUENCE: 126] Parse query type from command
query_type_t parse_query_type(const char* command) {
    if (strncmp(command, "QUERY ", 6) == 0) {
        return QUERY_SEARCH;
    } else if (strcmp(command, "STATS") == 0) {
        return QUERY_STATS;
    } else if (strcmp(command, "COUNT") == 0) {
        return QUERY_COUNT;
    } else if (strcmp(command, "HELP") == 0) {
        return QUERY_HELP;
    }
    return QUERY_UNKNOWN;
}

// [SEQUENCE: 264] Send help message
static void send_help(int client_fd) {
    const char* help_msg = 
        "LogCrafter Query Interface - MVP3\n"
        "================================\n"
        "Commands:\n"
        "  STATS                    - Show statistics\n"
        "  COUNT                    - Show log count\n"
        "  HELP                     - Show this help\n"
        "  QUERY param=value ...    - Enhanced search\n"
        "\n"
        "Query Parameters:\n"
        "  keyword=word             - Simple keyword search (legacy)\n"
        "  keywords=word1,word2     - Multiple keywords\n"
        "  regex=pattern            - POSIX regex pattern\n"
        "  time_from=timestamp      - Start time (Unix timestamp)\n"
        "  time_to=timestamp        - End time (Unix timestamp)\n"
        "  operator=AND|OR          - Keyword logic (default: AND)\n"
        "\n"
        "Examples:\n"
        "  QUERY keyword=error\n"
        "  QUERY regex=error.*timeout\n"
        "  QUERY keywords=error,warning operator=OR\n"
        "  QUERY time_from=1706140800 time_to=1706227200\n";
    
    send(client_fd, help_msg, strlen(help_msg), 0);
}

// [SEQUENCE: 127] Process query command - MVP3 version
void process_query_command(log_server_t* server, int client_fd, const char* command) {
    query_type_t type = parse_query_type(command);
    char response[BUFFER_SIZE];
    
    switch (type) {
        case QUERY_SEARCH: {
            // [SEQUENCE: 265] Use query parser for enhanced search
            parsed_query_t* query = query_parser_create();
            if (!query) {
                snprintf(response, sizeof(response), "ERROR: Memory allocation failed\n");
                send(client_fd, response, strlen(response), 0);
                break;
            }
            
            // Parse the query
            if (query_parser_parse(query, command) < 0) {
                snprintf(response, sizeof(response), "ERROR: Invalid query syntax\n");
                send(client_fd, response, strlen(response), 0);
                query_parser_destroy(query);
                break;
            }
            
            // [SEQUENCE: 266] Perform enhanced search
            char** results = NULL;
            int count = 0;
            
            if (log_buffer_search_enhanced(server->log_buffer, (const struct parsed_query*)query, &results, &count) == 0) {
                // Send search results
                snprintf(response, sizeof(response), "FOUND: %d matches\n", count);
                send(client_fd, response, strlen(response), 0);
                
                for (int i = 0; i < count; i++) {
                    snprintf(response, sizeof(response), "%s\n", results[i]);
                    send(client_fd, response, strlen(response), 0);
                    free(results[i]);
                }
                
                if (results) {
                    free(results);
                }
            } else {
                snprintf(response, sizeof(response), "ERROR: Search failed\n");
                send(client_fd, response, strlen(response), 0);
            }
            
            query_parser_destroy(query);
            break;
        }
        
        case QUERY_STATS: {
            // [SEQUENCE: 130] Get and send statistics
            unsigned long total, dropped;
            log_buffer_get_stats(server->log_buffer, &total, &dropped);
            size_t current = log_buffer_size(server->log_buffer);
            
            snprintf(response, sizeof(response), 
                    "STATS: Total=%lu, Dropped=%lu, Current=%zu, Clients=%d\n",
                    total, dropped, current, server->client_count);
            send(client_fd, response, strlen(response), 0);
            break;
        }
        
        case QUERY_COUNT: {
            // [SEQUENCE: 131] Send current log count
            size_t count = log_buffer_size(server->log_buffer);
            snprintf(response, sizeof(response), "COUNT: %zu\n", count);
            send(client_fd, response, strlen(response), 0);
            break;
        }
        
        case QUERY_HELP:
            send_help(client_fd);
            break;
        
        case QUERY_UNKNOWN:
        default:
            snprintf(response, sizeof(response), "ERROR: Unknown command. Use HELP for usage.\n");
            send(client_fd, response, strlen(response), 0);
            break;
    }
}
