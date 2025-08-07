#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "errors.h"
#include "symbol_table.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int run_preprocessor(const char* input_file, const char* am_file) {
    if (!preprocess_macros(input_file, am_file)) {
        fprintf(stderr, "Preprocessing failed for %s \n", input_file);
        return FALSE;
    }
    return TRUE;
}

static int run_first_pass(const char* am_file, SymbolTable* symtab, MemoryImage* memory) {
    if (first_pass(am_file, symtab, memory) == PASS_ERROR) {
        fprintf(stderr, "First pass failed for %s \n", am_file);
        return FALSE;
    }
    return TRUE;
}

static int run_second_pass(const char* am_file, SymbolTable* symtab, MemoryImage* memory, const char* obj_file, const char* ent_file, const char* ext_file) {
    if (second_pass(am_file, symtab, memory, obj_file, ent_file, ext_file) == PASS_ERROR) {
        fprintf(stderr, "Second pass failed for %s \n", am_file);
        return FALSE;
    }
    return TRUE;
}

static int process_input_file(const char* base_filename, const int file_number, const int total_files) {
    char input_file[MAX_FILENAME_LENGTH];
    char am_file[MAX_FILENAME_LENGTH];
    char obj_file[MAX_FILENAME_LENGTH], ent_file[MAX_FILENAME_LENGTH], ext_file[MAX_FILENAME_LENGTH];
    int result;
    SymbolTable symtab;
    MemoryImage memory;
    FILE *test_file;

    printf("\nProcessing file %d of %d: %s \n", file_number, total_files, base_filename);

    memset(&symtab, 0, sizeof(symtab));
    memset(&memory, 0, sizeof(memory));

    build_files(base_filename, input_file, am_file, obj_file, ent_file, ext_file);

    test_file = open_source_file(input_file); /* Verify input file exists */

    if (!test_file)
        return FALSE;
    
    safe_fclose(&test_file);

    result = (run_preprocessor(input_file, am_file) &&
                  run_first_pass(am_file, &symtab, &memory) &&
                  run_second_pass(am_file, &symtab, &memory, obj_file, ent_file, ext_file));
    
    free_symbol_table(&symtab);

    return result;
}

/* App main method */
/* ==================================================================== */
int main(int argc, char *argv[]) {
    int i, runtime_result;
    int success_count = 0, total_files = argc - 1;

    if (argc < 2) {
        fprintf(stderr, "Expected program call: %s <filename1> [filename2] ... \n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (process_input_file(argv[i], i, total_files)) 
            success_count++;
    }
    
    printf("\n --- Summary --- \n");
    printf("Successfully processed: %d/%d files \n", success_count, total_files);
    
    runtime_result = (success_count == total_files) ? 0 : 1;
    printf("Assembler completed with exit code %d! \n", runtime_result);

    return runtime_result;
}