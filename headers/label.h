#ifndef LABEL_H
#define LABEL_H

#define MAX_LABEL_LENGTH 31 /* Max label length (30 + null terminator) */

/* Function prototypes */

int is_valid_label(const char *label);
int is_reserved_word(const char *label);
int validate_label_syntax(const char *label);

#endif