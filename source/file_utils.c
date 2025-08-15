#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "file_io.h"

/*
Function to safely close a file pointer with null-checking and nullification.
Receives: FILE **fp - Pointer to file pointer (double pointer for modification)
*/
void safe_fclose(FILE **fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

/*
Function to open a source file for reading.
Receives: const char *filename - Path to source file
Returns: FILE* - Opened file pointer or NULL on failure
*/
FILE *open_source_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    
    if (!fp)
        print_error("Failed to open file", filename);
    
    return fp;
}

/*
Function to open a file for writing.
Receives: const char *filename - Path to output file
Returns: FILE* - Opened file pointer or NULL on failure
*/
FILE *open_output_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    
    if (!fp)
        print_error("Failed to write to file", filename);
    
    return fp;
}

/*
Function to write a formatted line to an output file.
Receives: FILE *fp - Valid file pointer (must be open for writing)
          const char *part1 - First part of line (cannot be NULL)
          const char *part2 - Second part of line (cannot be NULL)
*/
void write_file_line(FILE *fp, const char *part1, const char *part2) {
    fprintf(fp, "%s %s%c", part1, part2, NEWLINE);
}