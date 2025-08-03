#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbols.h"
#include "symbol_table.h"
#include "instructions.h"
#include "directives.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int validate_data_count(int memory_dc) {
    if (memory_dc >= WORD_COUNT) {
        print_error("Data section overflow", NULL);
        return FALSE;
    }
    return TRUE;
}

static int store_value(MemoryImage *memory, int value) {
    if (!validate_data_count(memory->dc))
        return FALSE;
    
    memory->words[INSTRUCTION_START + memory->ic + memory->dc].value = value & WORD_MASK;
    memory->words[INSTRUCTION_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
    memory->dc++;

    return TRUE;
}

static int validate_number(const char *token, int *value) {
    char *endptr;
    
    *value = strtol(token, &endptr, 10);

    if (*endptr != NULL_TERMINATOR) {
        print_error(ERR_INVALID_DATA, token);
        return FALSE;
    }
    
    /* Check range (-512 to 511 for 10-bit signed) */
    if (*value < -512 || *value > 511) {
        print_error(ERR_NUMBER_RANGE, token);
        return FALSE;
    }
    
    return TRUE;
}

static int validate_string_format(const char *str) {
    int len = strlen(str);
    
    if (len < 2 || str[0] != QUOTATION_CHAR || str[len-1] != QUOTATION_CHAR) {
        print_error(ERR_INVALID_STRING, str);
        return FALSE;
    }
    return TRUE;
}

static int parse_matrix_dimensions(const char *matrix_def, int *rows, int *cols) {
    if (sscanf(matrix_def, "[%d][%d]", rows, cols) != 2) {
        print_error(ERR_INVALID_MATRIX, matrix_def);
        return FALSE;
    }
    
    if (*rows <= 0 || *cols <= 0) {
        print_error(ERR_INVALID_MATRIX, matrix_def);
        return FALSE;
    }
    
    return TRUE;
}

static int process_data_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    int i, value;
    
    if (token_count <= start_idx + 1) {
        print_error(ERR_INVALID_DIR_DATA, NULL);
        return FALSE;
    }
    
    for (i = start_idx + 1; i < token_count; i++) {
        if (!validate_number(tokens[i], &value))
            return FALSE;
        
        if (!store_value(memory, value))
            return FALSE;
    }
    return TRUE;
}

static int process_string_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    char *str;
    int i, len;
    
    if (token_count != start_idx + 2) {
        print_error(ERR_INVALID_DIR_STRING, NULL);
        return FALSE;
    }
    
    str = tokens[start_idx + 1];
    if (!validate_string_format(str))
        return FALSE;
    
    len = strlen(str);
    
    /* Store characters without quotes */
    for (i = 1; i < len - 1; i++) {
        if (!store_value(memory, (unsigned char)str[i]))
            return FALSE;
    }
    
    /* Add null terminator */
    if (!store_value(memory, 0))
        return FALSE;
    
    return TRUE;
}

static int process_mat_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    char *matrix_def;
    int rows, cols, i, value;
    
    if (token_count <= start_idx + 2) {
        print_error(ERR_INVALID_DIR_MAT, NULL);
        return FALSE;
    }
    
    matrix_def = tokens[start_idx + 1];
    if (!parse_matrix_dimensions(matrix_def, &rows, &cols))
        return FALSE;
    
    /* Validate number of values */
    if (token_count != start_idx + 2 + (rows * cols)) {
        print_error(ERR_MATRIX_DIM_MISMATCH, NULL);
        return FALSE;
    }
    
    /* Store values */
    for (i = 0; i < rows * cols; i++) {
        if (!validate_number(tokens[start_idx + 2 + i], &value))
            return FALSE;
        
        if (!store_value(memory, value))
            return FALSE;
    }
    return TRUE;
}

static int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab) {
    int i;
    
    if (token_count != 2) {
        print_error(ERR_INVALID_DIR_ENTRY, NULL);
        return FALSE;
    }
    
    /* Find the symbol and mark it as entry */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, tokens[1]) == 0) {
            symtab->symbols[i].is_entry = TRUE;
            return TRUE;
        }
    }
    
    print_error(ERR_ENTRY_NOT_FOUND, tokens[1]);
    return FALSE;
}

static int process_extern_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab) {
    if (token_count != start_idx + 2) {
        print_error(ERR_INVALID_DIR_EXTERN, NULL);
        return FALSE;
    }
    
    return add_symbol(symtab, tokens[start_idx + 1], 0, EXTERNAL_SYMBOL);
}

/* Outer regular methods */
/* ==================================================================== */
int process_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int is_second_pass) {
    if (token_count < 1) 
        return FALSE;
    
    if (strcmp(tokens[0], DATA_DIRECTIVE) == 0) {
        if (is_second_pass) 
            return TRUE; /* Data already processed in first pass */

        return process_data_directive(tokens, token_count, 0, symtab, memory);
    }
    
    else if (strcmp(tokens[0], STRING_DIRECTIVE) == 0) {
        if (is_second_pass) 
            return TRUE; /* String already processed in first pass */

        return process_string_directive(tokens, token_count, 0, symtab, memory);
    }
    
    else if (strcmp(tokens[0], MATRIX_DIRECTIVE) == 0) {
        if (is_second_pass) 
            return TRUE; /* Matrix already processed in first pass */

        return process_mat_directive(tokens, token_count, 0, symtab, memory);
    }
    
    else if (strcmp(tokens[0], ENTRY_DIRECTIVE) == 0) {
        if (!is_second_pass) 
            return TRUE; /* Entry handled in second pass */

        return process_entry_directive(tokens, token_count, symtab);
    }
    
    else if (strcmp(tokens[0], EXTERN_DIRECTIVE) == 0) {
        if (is_second_pass) 
            return TRUE; /* Extern already processed in first pass */

        return process_extern_directive(tokens, token_count, 0, symtab);
    }
    
    print_error("Unknown directive", tokens[0]);

    return FALSE;
}