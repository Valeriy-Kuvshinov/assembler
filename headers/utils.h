#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

/* Function prototypes */

void safe_free(void **ptr);
void safe_fclose(FILE **fp);

/* String manipulation utilities */

void trim_whitespace(char* str);
char *dup_str(const char *src);

#endif