#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"

/*
Function to create a deep copy of a source string.
Receives: const char *src - Source string to be copied (NULL returns NULL)
Returns: char* - Newly allocated copy of the string, or NULL if allocation fails
*/
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

/*
Function to safely copy a string with bounds checking, preventing buffer overflow.
Receives: char *dest - Destination buffer
          const char *src - Source string to copy
          size_t dest_size - Size of destination buffer
          const char *context - Context for warning message (optional)
Returns: int - TRUE if full copy succeeded, FALSE if truncated
*/
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

/*
Function to remove leading and trailing whitespace from a string in-place.
Receives: char* str - String to be trimmed (modified directly)
*/
void trim_whitespace(char* str) {
    int i;
    int start = 0;
    int end = strlen(str) - 1;

    while (isspace(str[start])) {
        start++;
    }
    while (end >= start && isspace(str[end])) {
        end--;
    }
    for (i = 0; i <= end - start; i++) {
        str[i] = str[start + i];
    }
    str[i] = NULL_TERMINATOR;
}

/*
Function to remove comments (everything after ';') from a line in-place.
Receives: char *line - Input line (modified directly)
*/
void remove_comments(char *line) {
    char *comment_start = strchr(line, COMMENT_CHAR);

    if (comment_start)
        *comment_start = NULL_TERMINATOR;
}

/*
Function to prepare a line for processing by calling line hygine methods.
Receives: char *line - Input line (modified directly)
*/
void preprocess_line(char *line) {
    remove_comments(line);
    trim_whitespace(line);
}

/*
Function to print an error message to stdout with optional context.
Receives: const char *message - Main error message
          const char *context - Additional context (optional)
*/
void print_error(const char *message, const char *context) {
    if (context)
        printf("Error: %s (%s)%c", message, context, NEWLINE);
    else
        printf("Error: %s%c", message, NEWLINE);
}

/*
Function to print a line-specific error message with optional context.
Receives: const char *message - Main error message
          const char *context - Additional context (optional)
          int line_num - Source line number where error occurred
*/
void print_line_error(const char *message, const char *context, int line_num) {
    if (context)
        printf("Error: %s (%s) at line %d%c", message, context, line_num, NEWLINE);
    else
        printf("Error: %s at line %d%c", message, line_num, NEWLINE);
}

/*
Function to print a line-specific warning message with optional context.
Receives: const char *message - Warning message
          const char *context - Additional context (optional)
          int line_num - Source line number where warning occurred
*/
void print_line_warning(const char *message, const char *context, int line_num) {
    if (context)
        printf("Warning: %s (%s) at line %d%c", message, context, line_num, NEWLINE);
    else
        printf("Warning: %s at line %d%c", message, line_num, NEWLINE);
}

/*
Function to safely free memory and nullify the pointer.
Receives: void **ptr - Pointer to pointer that needs freeing
*/
void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}