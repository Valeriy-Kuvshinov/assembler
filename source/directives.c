#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbol_table.h"
#include "instructions.h"
#include "directives.h"

/* Inner STATIC methods */
/* ==================================================================== */
/*
Function validate a numeric token for .data directive.
Receives: const char *token - String to validate as number
          int *value - Output parameter for parsed value
Returns: int - TRUE if valid 10-bit signed integer (-512 to 511), FALSE otherwise
*/
static int check_number(const char *token, int *value) {
    char *endptr;
    
    *value = strtol(token, &endptr, BASE10_ENCODING);

    if ((*endptr != NULL_TERMINATOR) || (*value < -512 || *value > 511)) {
        print_error(".data value must be a decimal integer within the signed range (-512 to 511)", token);
        return FALSE;
    }
    return TRUE;
}

/*
Function to validate string literal format for .string directive.
Receives: const char *str - String to validate
Returns: int - TRUE if properly quoted (e.g., "text"), FALSE otherwise
*/
static int check_string(const char *str) {
    size_t length;

    length = strlen(str);

    if (str[0] != QUOTATION_CHAR || str[length - 1] != QUOTATION_CHAR) {
        print_error("String literal must be enclosed in double quotes", str);
        return FALSE;
    }
    return TRUE;
}

/*
Function to process .data directive - stores numeric values in memory.
Rules:
- Requires at least one numeric value
- Each value validated by check_number()
- Stores using store_value()
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table (unused)
          MemoryImage *memory - Memory image to store values
Returns: int - TRUE if all values stored successfully, FALSE otherwise
*/
static int process_data_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int i, value;
    int start_index = 0;

    if (token_count <= start_index + 1) {
        print_error("Invalid .data directive: need at least one numeric value", NULL);
        return FALSE;
    }

    for (i = start_index + 1; i < token_count; i++) {
        if (!check_number(tokens[i], &value))
            return FALSE;

        if (!store_value(memory, value))
            return FALSE;
    }
    return TRUE;
}

/*
Function to process .string directive - stores ASCII characters and null terminator.
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table (unused)
          MemoryImage *memory - Memory image to store string
Returns: int - TRUE if string stored successfully, FALSE otherwise
*/
static int process_string_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    char *str;
    int i;
    int start_index = 0;
    size_t length;

    if (token_count != start_index + 2) {
        print_error("Invalid .string directive: need exactly one string literal", NULL);
        return FALSE;
    }
    str = tokens[start_index + 1];

    if (!check_string(str))
        return FALSE;

    length = strlen(str);

    for (i = 1; i < length - 1; i++) {
        if (!store_value(memory, str[i]))
            return FALSE;
    }
    if (!store_value(memory, 0))
        return FALSE;

    return TRUE;
}

/*
Function to parse matrix dimensions from definition string.
Receives: const char *matrix_def - String like "[2][3]"
          int *rows - Output for row count
          int *cols - Output for column count
Returns: int - TRUE if valid dimensions (positive integers), FALSE otherwise
*/
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

/*
Function to Processes .mat directive - stores matrix values in row-major order.
Rules:
- Requires dimensions and exact number of values (rows*cols)
- Each value validated by check_number()
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table (unused)
          MemoryImage *memory - Memory image to store matrix
Returns: int - TRUE if matrix stored successfully, FALSE otherwise
*/
static int process_mat_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    char *matrix_def;
    int rows, cols, i, value, token_index;
    int start_index = 0;

    if (token_count <= start_index + 2) {
        print_error("Invalid .mat directive: need dimensions and at least one numeric value", NULL);
        return FALSE;
    }
    matrix_def = tokens[start_index + 1];

    if (!parse_matrix_dimensions(matrix_def, &rows, &cols))
        return FALSE;

    if (token_count != start_index + 2 + (rows * cols)) {
        print_error("Matrix dimensions don't match the number of provided values", NULL);
        return FALSE;
    }

    for (i = 0; i < rows * cols; i++) {
        token_index = start_index + 2 + i;

        if (!check_number(tokens[token_index], &value))
            return FALSE;

        if (!store_value(memory, value))
            return FALSE;
    }
    return TRUE;
}

/*
Function to process .entry directive - marks symbol as entry.
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table to modify
Returns: int - TRUE if symbol exists and marked, FALSE otherwisde
*/
static int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab) {
    Symbol *symbol_ptr;

    if (token_count != 2) {
        print_error("Invalid .entry directive: need exactly one label", NULL);
        return FALSE;
    }
    symbol_ptr = find_symbol(symtab, tokens[1]);

    if (symbol_ptr) {
        symbol_ptr->is_entry = TRUE;
        return TRUE;
    }
    return FALSE;
}

/*
Function to process .extern directive - declares external symbol.
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table to modify
Returns: int - TRUE if symbol added successfully, FALSE otherwise
*/
static int process_extern_directive(char **tokens, int token_count, SymbolTable *symtab) {
    int start_index = 0;

    if (token_count != start_index + 2) {
        print_error("Invalid .extern directive: need exactly one label", NULL);
        return FALSE;
    }
    return add_symbol(symtab, tokens[start_index + 1], 0, EXTERNAL_SYMBOL);
}

/* Outer methods */
/* ==================================================================== */
/*
Function to route to the relevant directive handling.
Handles:
- .data/.string/.mat in first pass only
- .entry in second pass only
- .extern in first pass only
Receives: char **tokens - Tokenized directive line
          int token_count - Number of tokens
          SymbolTable *symtab - Symbol table for symbol operations
          MemoryImage *memory - Memory image for data storage
          int is_second_pass - Assembly phase flag
Returns: int - TRUE if directive processed successfully, FALSE otherwise
*/
int process_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int is_second_pass) {
    if (strcmp(tokens[0], DATA_DIRECTIVE) == 0) {
        if (is_second_pass)
            return TRUE;

        return process_data_directive(tokens, token_count, symtab, memory);
    }
    else if (strcmp(tokens[0], STRING_DIRECTIVE) == 0) {
        if (is_second_pass)
            return TRUE;

        return process_string_directive(tokens, token_count, symtab, memory);
    }
    else if (strcmp(tokens[0], MATRIX_DIRECTIVE) == 0) {
        if (is_second_pass)
            return TRUE;

        return process_mat_directive(tokens, token_count, symtab, memory);
    }
    else if (strcmp(tokens[0], ENTRY_DIRECTIVE) == 0) {
        if (!is_second_pass)
            return TRUE;

        return process_entry_directive(tokens, token_count, symtab);
    }
    else if (strcmp(tokens[0], EXTERN_DIRECTIVE) == 0) {
        if (is_second_pass)
            return TRUE;

        return process_extern_directive(tokens, token_count, symtab);
    }
    return FALSE;
}