#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "file_io.h"

void build_files(const char* base_filename, char* input_file, char* am_file, char* obj_file, char* ent_file, char* ext_file) {
    sprintf(input_file, "%s%s", base_filename, FILE_EXT_INPUT);
    sprintf(am_file, "%s%s", base_filename, FILE_EXT_PREPROC);
    sprintf(obj_file, "%s%s", base_filename, FILE_EXT_OBJECT);
    sprintf(ent_file, "%s%s", base_filename, FILE_EXT_ENTRY);
    sprintf(ext_file, "%s%s", base_filename, FILE_EXT_EXTERN);
}

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