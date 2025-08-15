#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"

char *copy_string(const char *src) {
    char *copy;
    size_t length;

    if (!src) 
        return NULL;

    length = strlen(src);
    copy = malloc(length + 1);

    if (copy)
        strcpy(copy, src);

    return copy;
}

int bounded_string_copy(char *dest, const char *src, size_t dest_size, const char *context) {
    size_t src_length;

    if (!dest || !src || dest_size == 0)
        return FALSE;

    src_length = strlen(src);

    if (src_length >= dest_size) {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = NULL_TERMINATOR;

        if (context)
            printf("Warning: String truncated in %s%c", context, NEWLINE);

        return FALSE;
    }
    strcpy(dest, src);
    return TRUE;
}

void trim_whitespace(char* str) {
    int i;
    int start = 0;
    int end = strlen(str) - 1;

    /* Find first non-whitespace */
    while (isspace(str[start])) {
        start++;
    }
    /* Find last non-whitespace */
    while (end >= start && isspace(str[end])) {
        end--;
    }
    /* Shift characters */
    for (i = 0; i <= end - start; i++) {
        str[i] = str[start + i];
    }
    str[i] = NULL_TERMINATOR;
}

void remove_comments(char *line) {
    char *comment_start = strchr(line, COMMENT_CHAR);

    if (comment_start)
        *comment_start = NULL_TERMINATOR;
}

void preprocess_line(char *line) {
    remove_comments(line);
    trim_whitespace(line);
}

int has_label_in_tokens(char **tokens, int token_count) {
    /* Only check first token for label */
    return (token_count > 0 && strchr(tokens[0], LABEL_TERMINATOR));
}

void print_error(const char *message, const char *context) {
    if (context)
        printf("Error: %s (%s)%c", message, context, NEWLINE);
    else
        printf("Error: %s%c", message, NEWLINE);
}

void print_line_error(const char *message, const char *context, int line_num) {
    if (context)
        printf("Error: %s (%s) at line %d%c", message, context, line_num, NEWLINE);
    else
        printf("Error: %s at line %d%c", message, line_num, NEWLINE);
}

void print_line_warning(const char *message, const char *context, int line_num) {
    if (context)
        printf("Warning: %s (%s) at line %d%c", message, context, line_num, NEWLINE);
    else
        printf("Warning: %s at line %d%c", message, line_num, NEWLINE);
}

void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}