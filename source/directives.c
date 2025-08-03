#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbols.h"
#include "instructions.h"
#include "directives.h"

int process_data_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    char *endptr;
    int i, value;
    
    if (token_count <= start_idx + 1) {
        print_error("Missing data values", NULL);
        return FALSE;
    }
    
    for (i = start_idx + 1; i < token_count; i++) {
        value = strtol(tokens[i], &endptr, 10);

        if (*endptr != NULL_TERMINATOR) {
            print_error("Invalid data value", tokens[i]);
            return FALSE;
        }
        
        /* Store data word */
        if (memory->dc >= WORD_COUNT) {
            print_error("Data section overflow", NULL);
            return FALSE;
        }
        
        /* Actually store the value */
        memory->words[IC_START + memory->ic + memory->dc].value = value & WORD_MASK;
        memory->dc++;
    }
    return TRUE;
}

int process_string_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    char *str;
    int i, len;
    
    if (token_count != start_idx + 2) {
        print_error("Invalid string directive", NULL);
        return FALSE;
    }
    
    str = tokens[start_idx + 1];
    len = strlen(str);
    
    /* Remove quotes and store each character */
    if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
        /* Store characters without quotes */
        for (i = 1; i < len - 1; i++) {
            if (memory->dc >= WORD_COUNT) {
                print_error("Data section overflow", NULL);
                return FALSE;
            }
            memory->words[IC_START + memory->ic + memory->dc].value = (unsigned char)str[i];
            memory->words[IC_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
            memory->dc++;
        }
    } else {
        print_error("Invalid string format - missing quotes", str);
        return FALSE;
    }
    
    /* Add null terminator */
    if (memory->dc >= WORD_COUNT) {
        print_error("Data section overflow", NULL);
        return FALSE;
    }
    memory->words[IC_START + memory->ic + memory->dc].value = 0;
    memory->words[IC_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
    memory->dc++;
    
    return TRUE;
}

int process_mat_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    char *matrix_def;
    int rows, cols, i;
    
    /* Must have at least .mat [rows][cols] and one value */
    if (token_count <= start_idx + 2) {
        print_error("Invalid matrix directive", NULL);
        return FALSE;
    }
    
    matrix_def = tokens[start_idx + 1];
    
    /* Parse [rows][cols] */
    if (sscanf(matrix_def, "[%d][%d]", &rows, &cols) != 2) {
        print_error("Invalid matrix dimensions", matrix_def);
        return FALSE;
    }
    
    /* Validate dimensions */
    if (rows <= 0 || cols <= 0) {
        print_error("Invalid matrix dimensions", matrix_def);
        return FALSE;
    }
    
    /* Validate number of values */
    if (token_count != start_idx + 2 + (rows * cols)) {
        print_error("Incorrect number of matrix values", NULL);
        return FALSE;
    }
    
    /* Store values */
    for (i = 0; i < rows * cols; i++) {
        char *endptr;
        int value = strtol(tokens[start_idx + 2 + i], &endptr, 10);
        
        if (*endptr != NULL_TERMINATOR) {
            print_error("Invalid matrix value", tokens[start_idx + 2 + i]);
            return FALSE;
        }
        
        if (memory->dc >= WORD_COUNT) {
            print_error("Data section overflow", NULL);
            return FALSE;
        }
        
        memory->words[IC_START + memory->ic + memory->dc].value = value & WORD_MASK;
        memory->dc++;
    }
    return TRUE;
}

int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab) {
    int i;
    
    if (token_count != 2) {
        print_error("Invalid entry directive", NULL);
        return FALSE;
    }
    
    /* Find the symbol and mark it as entry */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, tokens[1]) == 0) {
            symtab->symbols[i].is_entry = TRUE;
            return TRUE;
        }
    }
    print_error(ERR_ENTRY_LABEL_NOT_FOUND, tokens[1]);

    return FALSE;
}