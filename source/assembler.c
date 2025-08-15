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
/*
Function to construct full filenames by combining base name with standard extensions.
Generates filenames for: input (.as), preprocessed (.am), object (.ob), entry (.ent), and external (.ext) files
Receives: const char* base_filename - Base filename without extension
          char* input_file - Output buffer for input filename
          char* am_file - Output buffer for preprocessed filename
          char* obj_file - Output buffer for object filename
          char* ent_file - Output buffer for entry filename
          char* ext_file - Output buffer for externals filename
Returns: void
*/
static void build_files(const char* base_filename, char* input_file, char* am_file, char* obj_file, char* ent_file, char* ext_file) {
    sprintf(input_file, "%s%s", base_filename, FILE_EXT_INPUT);
    sprintf(am_file, "%s%s", base_filename, FILE_EXT_PREPROC);
    sprintf(obj_file, "%s%s", base_filename, FILE_EXT_OBJECT);
    sprintf(ent_file, "%s%s", base_filename, FILE_EXT_ENTRY);
    sprintf(ext_file, "%s%s", base_filename, FILE_EXT_EXTERN);
}

/*
Function to process a single .as file through the complete assembler pipeline:
- Preprocessing (macro expansion)
- First pass (symbol table creation)
- Second pass (code encoding & output)
Receives: const char* base_filename - Base filename without extension
          const int file_number - Current file index (for progress display)
          const int total_files - Total files to process
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          MacroTable *macrotab - Pointer to macro table
Returns: int - TRUE if file processed successfully, FALSE on any error
*/
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

    if (preprocess_macros(input_file, am_file, macrotab) == PASS_ERROR) {
        printf("%cPreprocessing failed for %s%c", NEWLINE, input_file, NEWLINE);
        return FALSE;
    }
    if (first_pass(am_file, symtab, memory) == PASS_ERROR) {
        printf("%cFirst pass failed for %s%c", NEWLINE, am_file, NEWLINE);
        return FALSE;
    }
    if (second_pass(am_file, symtab, memory, obj_file, ent_file, ext_file) == PASS_ERROR) {
        printf("%cSecond pass failed for %s%c", NEWLINE, am_file, NEWLINE);
        return FALSE;
    }
    return TRUE;
}

/* App main method */
/* ==================================================================== */
/*
Main entry point to the assembler program.
Processes multiple assembly files through complete pipeline.
Manages initialization and cleanup of data structures.
Displays summary upon completion.
Receives: int argc - Number of command line arguments
          char *argv[] - Array of command line arguments
                         (argv[1..n] are input filenames)
Returns: int - 0 if all files processed successfully, 1 otherwise
*/
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