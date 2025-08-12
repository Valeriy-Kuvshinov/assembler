#ifndef FILE_IO_H
#define FILE_IO_H

#include "memory.h"
#include "symbol_table.h"
#include "macro_table.h"

/* Various file extensions */
#define FILE_EXT_INPUT ".as"
#define FILE_EXT_PREPROC ".am"
#define FILE_EXT_OBJECT ".ob"
#define FILE_EXT_ENTRY ".ent"
#define FILE_EXT_EXTERN ".ext"

#define MAX_FILENAME_LENGTH 100

#define PASS_ERROR -1 /* Pass result code */

/* Function prototypes */

void safe_fclose(FILE **fp);

FILE *open_source_file(const char *filename);

FILE *open_output_file(const char *filename);

int preprocess_macros(const char* src_filename, const char* am_filename, MacroTable *macro_table);

int first_pass(const char *filename, SymbolTable *symbol_table, MemoryImage *memory);

int second_pass(
    const char *filename, SymbolTable *symbol_table, MemoryImage *memory,
    const char *obj_file, const char *ent_file, const char *ext_file
);

void write_object_file(const char *filename, MemoryImage *memory);

void write_entry_file(const char *filename, SymbolTable *symbol_table);

void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symbol_table);

#endif