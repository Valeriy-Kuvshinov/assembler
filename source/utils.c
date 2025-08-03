#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"

void print_error(const char *message, const char *context) {
    if (context)
        fprintf(stderr, "Error: %s (%s) \n", message, context);
    else
        fprintf(stderr, "Error: %s \n", message);
}

void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

void safe_fclose(FILE **fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

void trim_whitespace(char* str) {
    int i; 
    int start = 0, end = strlen(str) - 1;
    
    /* Find first non-whitespace */
    while (isspace((unsigned char)str[start])) 
        start++;
    
    /* Find last non-whitespace */
    while (end >= start && isspace((unsigned char)str[end])) 
        end--;
    
    /* Shift characters */
    for (i = 0; i <= end - start; i++) {
        str[i] = str[start + i];
    }
    str[i] = NULL_TERMINATOR;
}

char *dup_str(const char *src) {
    char *copy = malloc(strlen(src) + 1);

    if (copy)
        strcpy(copy, src);

    return copy;
}

int should_skip_line(const char *line) {
    return (line[0] == NULL_TERMINATOR || line[0] == COMMENT_CHAR);
}

void remove_comments(char *line) {
    char *comment_start = strchr(line, COMMENT_CHAR);

    if (comment_start)
        *comment_start = NULL_TERMINATOR;
}