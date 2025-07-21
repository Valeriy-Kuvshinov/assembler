#include <stdio.h>

#include "errors.h"

void print_error(const char *message, const char *context) {
    if (context)
        fprintf(stderr, "Error: %s (%s) \n", message, context);
    else
        fprintf(stderr, "Error: %s \n", message);
}