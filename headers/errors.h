#ifndef ERRORS_H
#define ERRORS_H

/* Error codes */
#define ERR_MEMORY_ALLOCATION "Memory allocation failed"
#define ERR_ILLEGAL_COMMA "Illegal comma"
#define ERR_CONSECUTIVE_COMMAS "Consecutive commas"
#define ERR_MACRO_OVERFLOW "Macro body exceeded maximum size"
#define ERR_INVALID_FILENAME "Input filename must not have an extension"
#define ERR_MISSING_MACRO_NAME "Missing macro name"
#define ERR_INVALID_MACRO_NAME "Invalid macro name"
#define ERR_DUPLICATE_MACRO "Duplicate macro name"
#define ERR_UNEXPECTED_TOKEN "Unexpected token"

/* Function prototypes */

void print_error(const char *message, const char *context);

#endif