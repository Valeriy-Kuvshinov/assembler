#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define MAX_LINE_LENGTH 81  /* 80 chars + null terminator */

/* 'Boolean' Constants */
#define FALSE 0
#define TRUE !FALSE

/* Char constants */
#define COMMENT_CHAR ';'
#define COMMA_CHAR ','
#define DIRECTIVE_CHAR '.'
#define UNDERSCORE_CHAR '_'
#define LABEL_TERMINATOR ':'
#define IMMEDIATE_PREFIX '#'
#define LEFT_BRACKET '['
#define RIGHT_BRACKET ']'
#define QUOTATION_CHAR '"'
#define SPACE_TAB " \t"
#define SPACE_TAB_NEWLINE " \t\n"

#define NULL_TERMINATOR '\0'

#define BASE10_ENCODING 10

/* Function prototypes */

void trim_whitespace(char* str);

void remove_comments(char *line);

void preprocess_line(char *line);

int should_skip_line(const char *line);

void print_error(const char *message, const char *context);

void safe_free(void **ptr);

char *clone_string(const char *src);

#endif