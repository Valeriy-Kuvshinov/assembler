#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "file_io.h"

void safe_fclose(FILE **fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

FILE *open_source_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    
    if (!fp)
        print_error("Failed to open file", filename);
    
    return fp;
}

FILE *open_output_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    
    if (!fp)
        print_error("Failed to write to file", filename);
    
    return fp;
}