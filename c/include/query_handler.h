#ifndef QUERY_HANDLER_H
#define QUERY_HANDLER_H

#include "server.h"
#include "query_parser.h"

// [SEQUENCE: 122] Query command structure
typedef enum {
    QUERY_SEARCH,
    QUERY_STATS,
    QUERY_COUNT,
    QUERY_HELP,
    QUERY_UNKNOWN
} query_type_t;

// [SEQUENCE: 123] Query handler functions
void handle_query_connection(log_server_t* server);
void process_query_command(log_server_t* server, int client_fd, const char* command);
query_type_t parse_query_type(const char* command);

// [SEQUENCE: 258] Enhanced search with parser
int search_logs_enhanced(log_server_t* server, parsed_query_t* query, char*** results);

#endif // QUERY_HANDLER_H
