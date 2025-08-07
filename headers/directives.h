#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "memory.h"
#include "symbol_table.h"

/* Data directives */
#define DATA_DIRECTIVE ".data"
#define STRING_DIRECTIVE ".string"
#define MATRIX_DIRECTIVE ".mat"
/* Linkers directives */
#define ENTRY_DIRECTIVE ".entry"
#define EXTERN_DIRECTIVE ".extern"

/* Function prototypes */

int process_directive(
    char **tokens, int token_count, SymbolTable *symtab,
    MemoryImage *memory, int is_second_pass
);

/* Validation macros */

#define IS_DATA_DIRECTIVE(token) \
    (strcmp((token), DATA_DIRECTIVE) == 0 || strcmp((token), STRING_DIRECTIVE) == 0 || strcmp((token), MATRIX_DIRECTIVE) == 0)

#define IS_LINKER_DIRECTIVE(token) \
    (strcmp((token), ENTRY_DIRECTIVE) == 0 || strcmp((token), EXTERN_DIRECTIVE) == 0)

#endif