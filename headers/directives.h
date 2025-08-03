#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "memory.h"
#include "symbols.h"

/* Data Directives */
#define DATA_DIRECTIVE ".data"
#define STRING_DIRECTIVE ".string"
#define MATRIX_DIRECTIVE ".mat"
/* Symbol Visibility Directives */
#define ENTRY_DIRECTIVE ".entry"
#define EXTERN_DIRECTIVE ".extern"

/* Function prototypes */

int process_data_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_string_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_mat_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory);
int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab);

#endif