#ifndef FILE_IO_H
#define FILE_IO_H

#include "memory.h"
#include "symbols.h"

#define FILE_EXT_INPUT ".as"
#define FILE_EXT_PREPROC ".am"
#define FILE_EXT_OBJECT ".ob"
#define FILE_EXT_ENTRY ".ent"
#define FILE_EXT_EXTERN ".ext"

#define MAX_FILENAME_LENGTH 100

/* First pass result codes */
#define PASS_SUCCESS 0
#define PASS_ERROR -1

/* Function prototypes */

int process_file(const char* base_filename, int file_number, int total_files);

int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory);

int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory, const char *obj_file, const char *ent_file, const char *ext_file);

#endif