#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbol_table.h"
#include "macro_table.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void build_files(const char* base_filename, char* input_file, char* am_file, char* obj_file, char* ent_file, char* ext_file) {
    sprintf(input_file, "%s%s", base_filename, FILE_EXT_INPUT);
    sprintf(am_file, "%s%s", base_filename, FILE_EXT_PREPROC);
    sprintf(obj_file, "%s%s", base_filename, FILE_EXT_OBJECT);
    sprintf(ent_file, "%s%s", base_filename, FILE_EXT_ENTRY);
    sprintf(ext_file, "%s%s", base_filename, FILE_EXT_EXTERN);
}

static int run_preprocessor(const char* input_file, const char* am_file, MacroTable *macrotab) {
    if (!preprocess_macros(input_file, am_file, macrotab)) {
        print_error("Preprocessing failed", input_file);
        return FALSE;
    }
    return TRUE;
}

static int run_first_pass(const char* am_file, SymbolTable *symtab, MemoryImage *memory) {
    if (first_pass(am_file, symtab, memory) == PASS_ERROR) {
        print_error("First pass failed", am_file);
        return FALSE;
    }
    return TRUE;
}

static int run_second_pass(const char* am_file, SymbolTable *symtab, MemoryImage *memory, const char* obj_file, const char* ent_file, const char* ext_file) {
    if (second_pass(am_file, symtab, memory, obj_file, ent_file, ext_file) == PASS_ERROR) {
        print_error("Second pass failed", am_file);
        return FALSE;
    }
    return TRUE;
}

static int process_input_file(const char* base_filename, const int file_number, const int total_files, SymbolTable *symtab, MemoryImage *memory, MacroTable *macrotab) {
    char input_file[MAX_FILENAME_LENGTH];
    char am_file[MAX_FILENAME_LENGTH];
    char obj_file[MAX_FILENAME_LENGTH], ent_file[MAX_FILENAME_LENGTH], ext_file[MAX_FILENAME_LENGTH];
    FILE *test_file;

    printf("%cProcessing file %d of %d: %s%c", NEWLINE, file_number, total_files, base_filename, NEWLINE);
    build_files(base_filename, input_file, am_file, obj_file, ent_file, ext_file);

    test_file = open_source_file(input_file);

    if (!test_file) {
        print_error("File not found", input_file);
        return FALSE;
    }
    safe_fclose(&test_file);

    return (run_preprocessor(input_file, am_file, macrotab) &&
            run_first_pass(am_file, symtab, memory) &&
            run_second_pass(am_file, symtab, memory, obj_file, ent_file, ext_file));
}

/* App main method */
/* ==================================================================== */
int main(int argc, char *argv[]) {
    int i, runtime_result;
    int success_count = 0, total_files = argc - 1;

    if (argc < 2) {
        print_error("Expected different app call", "./assembler <filename1> [filename2] ...");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        SymbolTable symtab;
        MemoryImage memory;
        MacroTable macrotab;

        init_memory(&memory);

        if (!init_symbol_table(&symtab)) {
            print_error("Failed to initialize symbol table for file", argv[i]);
            continue;
        }

        if (!init_macro_table(&macrotab)) {
            print_error("Failed to initialize macro table for file", argv[i]);
            free_symbol_table(&symtab);
            continue;
        }

        if (process_input_file(argv[i], i, total_files, &symtab, &memory, &macrotab))
            success_count++;

        free_symbol_table(&symtab);
        free_macro_table(&macrotab);
    }
    printf("%c--- Summary ---%c", NEWLINE, NEWLINE);
    printf("Successfully processed: %d/%d files%c", success_count, total_files, NEWLINE);

    runtime_result = (success_count == total_files) ? 0 : 1;
    printf("%cAssembler completed with exit code %d!%c", NEWLINE, runtime_result, NEWLINE);

    return runtime_result;
}