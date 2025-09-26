#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void test_nested_strtok_r() {
    printf("Testing nested strtok_r\n");
    
    char* str = strdup("keywords=ERROR,WARNING operator=OR");
    char* saveptr1;
    char* token = strtok_r(str, " ", &saveptr1);
    
    int token_num = 0;
    while (token) {
        printf("Outer token %d: '%s'\n", ++token_num, token);
        
        // Parse inner tokens
        if (strncmp(token, "keywords=", 9) == 0) {
            char* value = token + 9;
            char* value_copy = strdup(value);
            char* saveptr2;
            char* inner = strtok_r(value_copy, ",", &saveptr2);
            
            while (inner) {
                printf("  Inner token: '%s'\n", inner);
                inner = strtok_r(NULL, ",", &saveptr2);
            }
            free(value_copy);
            printf("  Done with inner parsing\n");
        }
        
        token = strtok_r(NULL, " ", &saveptr1);
        printf("Next outer token: %s\n", token ? token : "NULL");
    }
    
    free(str);
}

int main() {
    test_nested_strtok_r();
    return 0;
}