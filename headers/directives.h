#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "memory.h"
#include "symbol_table.h"

/* Function prototypes */

int process_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int is_second_pass);

#endif