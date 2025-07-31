#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define MAX_LINE_LENGTH 81  /* 80 chars + null terminator */

/* Boolean Constants */
#define FALSE 0
#define TRUE !FALSE

/* Input Parsing Constants */
#define COMMENT_CHAR ';'
#define DIRECTIVE_CHAR '.'
#define UNDERSCORE_CHAR '_'
#define LABEL_TERMINATOR ':'
#define IMMEDIATE_PREFIX '#'

/* Function prototypes */

void print_error(const char *message, const char *context);
void safe_free(void **ptr);
void safe_fclose(FILE **fp);
void trim_whitespace(char* str);
char *dup_str(const char *src);

#endif