#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/query_parser.h"

int main() {
    // Test query parsing
    const char* test_query = "QUERY keywords=ERROR,WARNING operator=OR";
    printf("Test query: '%s' (len=%zu)\n", test_query, strlen(test_query));
    
    parsed_query_t* query = query_parser_create();
    if (!query) {
        printf("Failed to create query\n");
        return 1;
    }
    
    // Debug: Manually tokenize to see what happens
    printf("\n--- Manual tokenization ---\n");
    char* debug_copy = strdup("keywords=ERROR,WARNING operator=OR");
    char* tok = strtok(debug_copy, " ");
    while (tok) {
        printf("Token: '%s'\n", tok);
        tok = strtok(NULL, " ");
    }
    free(debug_copy);
    printf("--- End manual tokenization ---\n\n");
    
    int result = query_parser_parse(query, test_query);
    printf("Parse result: %d\n", result);
    printf("Keyword count: %d\n", query->keyword_count);
    printf("Operator: %d (AND=0, OR=1, NONE=2)\n", query->op);
    
    for (int i = 0; i < query->keyword_count; i++) {
        printf("Keyword %d: '%s'\n", i, query->keywords[i]);
    }
    
    // Test matching
    // Test parsing second query with operator
    printf("\n--- Testing operator parsing ---\n");
    const char* test_query2 = "QUERY operator=OR";
    parsed_query_t* query2 = query_parser_create();
    query_parser_parse(query2, test_query2);
    printf("Query2 operator: %d (should be 1 for OR)\n", query2->op);
    query_parser_destroy(query2);
    
    const char* test_logs[] = {
        "ERROR: Database failed",
        "WARNING: High memory",
        "INFO: All OK"
    };
    
    for (int i = 0; i < 3; i++) {
        bool matches = query_matches_log(query, test_logs[i], 0);
        printf("Log '%s' matches: %s\n", test_logs[i], matches ? "YES" : "NO");
    }
    
    query_parser_destroy(query);
    return 0;
}