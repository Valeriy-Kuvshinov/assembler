#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "memory.h"
#include "symbol_table.h"

/* Function prototypes */

int process_data_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_string_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_mat_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab);
int process_extern_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab);

#endif