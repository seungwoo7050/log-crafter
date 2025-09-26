#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    const char* query = "keywords=ERROR,WARNING operator=OR";
    char* copy = strdup(query);
    
    printf("Original: '%s'\n", query);
    printf("Copy: '%s'\n", copy);
    
    char* token = strtok(copy, " ");
    int count = 0;
    while (token) {
        printf("Token %d: '%s'\n", ++count, token);
        token = strtok(NULL, " ");
    }
    
    free(copy);
    return 0;
}