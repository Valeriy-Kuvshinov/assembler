#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

/* Boolean Constants */
#define FALSE 0
#define TRUE !FALSE

/* Function prototypes */

void print_error(const char *message, const char *context);
void safe_free(void **ptr);
void safe_fclose(FILE **fp);
void trim_whitespace(char* str);
char *dup_str(const char *src);

#endif