#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "utils.h"
#include "files.h"
#include "file_process.h"
#include "file_passer.h"
#include "pre_assembler.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int verify_input_file(const char* filename) {
    FILE* test_file = fopen(filename, "r");

    if (!test_file) {
        fprintf(stderr, "Error: Input file not found - %s\n", filename);
        return FALSE;
    }
    fclose(test_file);
    return TRUE;
}

static void build_filenames(
                            const char* base_filename, char* input_file, char* am_file, 
                            char* obj_file, char* ent_file, char* ext_file) {
    sprintf(input_file, "%s%s", base_filename, FILE_EXT_INPUT);
    sprintf(am_file, "%s%s", base_filename, FILE_EXT_PREPROC);
    sprintf(obj_file, "%s%s", base_filename, FILE_EXT_OBJECT);
    sprintf(ent_file, "%s%s", base_filename, FILE_EXT_ENTRY);
    sprintf(ext_file, "%s%s", base_filename, FILE_EXT_EXTERN);
}

static int run_preprocessor(const char* input_file, const char* am_file) {
    if (!preprocess_macros(input_file, am_file)) {
        fprintf(stderr, "Preprocessing failed for %s\n", input_file);
        return FALSE;
    }
    printf("Preprocessing passed: %s -> %s\n", input_file, am_file);
    return TRUE;
}

static int run_first_pass(const char* am_file, SymbolTable* symtab, MemoryImage* memory) {
    if (first_pass(am_file, symtab, memory) == PASS_ERROR) {
        fprintf(stderr, "First pass failed for %s\n", am_file);
        return FALSE;
    }
    printf("First pass passed: %s\n", am_file);
    return TRUE;
}

static int run_second_pass(
                            const char* am_file, SymbolTable* symtab, MemoryImage* memory,
                            const char* obj_file, const char* ent_file, const char* ext_file) {
    if (second_pass(am_file, symtab, memory, obj_file, ent_file, ext_file) == PASS_ERROR) {
        fprintf(stderr, "Second pass failed for %s\n", am_file);
        return FALSE;
    }
    printf("Second pass passed: %s -> %s\n", am_file, obj_file);
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
/* Process a single input file */
int process_file(const char* base_filename, int file_number, int total_files) {
    char input_file[MAX_FILENAME_LENGTH];
    char am_file[MAX_FILENAME_LENGTH];
    char obj_file[MAX_FILENAME_LENGTH], ent_file[MAX_FILENAME_LENGTH], ext_file[MAX_FILENAME_LENGTH];
    SymbolTable symtab;
    MemoryImage memory;

    printf("\nProcessing file %d of %d: %s\n", file_number, total_files, base_filename);

    memset(&symtab, 0, sizeof(symtab));
    memset(&memory, 0, sizeof(memory));

    build_filenames(base_filename, input_file, am_file, obj_file, ent_file, ext_file);

    /* Verify input file exists */
    if (!verify_input_file(input_file)) 
        return FALSE;

    /* Execute processing pipeline */
    return (run_preprocessor(input_file, am_file) &&
           run_first_pass(am_file, &symtab, &memory) &&
           run_second_pass(am_file, &symtab, &memory, obj_file, ent_file, ext_file));
}