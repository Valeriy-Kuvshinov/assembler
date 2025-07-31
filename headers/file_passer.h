#ifndef FILE_PASSER_H
#define FILE_PASSER_H

#include "memory_layout.h"

/* First pass result codes */
#define PASS_SUCCESS 0
#define PASS_ERROR -1

/* Function prototypes */

int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory);
int second_pass(
                const char *filename, SymbolTable *symtab, MemoryImage *memory, 
                const char *obj_file, const char *ent_file, const char *ext_file);

#endif