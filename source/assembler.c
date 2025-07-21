#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "errors.h"
#include "utils.h"
#include "pre_assembler.h"

/* Handle preprocessing phase for a single file */
static int handle_preprocessing(const char* input_file_with_ext, const char* output_file) {
    if (!preprocess_macros(input_file_with_ext, output_file)) {
        fprintf(stderr, "Preprocessing failed for %s \n", input_file_with_ext);
        return FALSE;
    } else {
        printf("Success: %s -> %s \n", input_file_with_ext, output_file);
        return TRUE;
    }
}

/* Process a single input file */
static int process_file(const char* input_file, int file_number, int total_files) {
    char input_file_with_ext[WORD_COUNT];
    char output_file[WORD_COUNT];
    char *dot_pos;
    FILE *test_file;
    
    printf("\n --- Processing file %d of %d --- \n", file_number, total_files);
    
    dot_pos = strrchr(input_file, '.'); /* Check if input file has any extension */

    if (dot_pos != NULL) {
        print_error(ERR_INVALID_FILENAME, input_file);
        fprintf(stderr, "Please use: assembler <basefilename> \n");
        return FALSE;
    }

    /* Create input filename with .as extension */
    sprintf(input_file_with_ext, "%s%s", input_file, FILE_EXT_INPUT);
    
    /* Check if input file exists */
    test_file = fopen(input_file_with_ext, "r");

    if (test_file) {
        printf("Found: %s \n", input_file_with_ext);
        safe_fclose(&test_file);
    } else 
        printf("Creating: %s \n", input_file_with_ext);
    
    /* Create output filename with .am extension */
    sprintf(output_file, "%s%s", input_file, FILE_EXT_PREPROC);
    
    /* Phase 1: Preprocessing */
    if (!handle_preprocessing(input_file_with_ext, output_file)) 
        return FALSE;
    
    /* Phase 2: First pass (future implementation) */
    /* if (!handle_first_pass(output_file, symbol_table)) {
        return FALSE;
    } */
    
    /* Phase 3: Second pass (future implementation) */
    /* if (!handle_second_pass(output_file, symbol_table)) {
        return FALSE;
    } */
    
    return TRUE;
}

int main(int argc, char *argv[]) {
    int i, runtime_result;
    int success_count = 0;
    int total_files = argc - 1;

    if (argc < 2) {
        fprintf(stderr, "Expected program call: %s <inputfile1> [inputfile2] ... \n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (process_file(argv[i], i, total_files)) 
            success_count++;
    }
    
    printf("\n --- Summary --- \n");
    printf("Successfully processed: %d/%d files \n", success_count, total_files);
    
    runtime_result = (success_count == total_files) ? 0 : 1;
    printf("Assembler completed with exit code %d! \n", runtime_result);

    return runtime_result;
}