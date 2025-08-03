#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"
#include "label.h"
#include "symbol_table.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int validate_symbol_addition(const SymbolTable *symtab, const char *name, int type) {
    int i;
    
    /* Check for duplicates and conflicts */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0) {
            /* Check for conflicting attributes */
            if ((symtab->symbols[i].type == EXTERNAL_SYMBOL && type == ENTRY_SYMBOL) ||
                (symtab->symbols[i].type == ENTRY_SYMBOL && type == EXTERNAL_SYMBOL)) {
                print_error(ERR_ENTRY_EXTERN_CONFLICT, name);
                return FALSE;
            }
            print_error(ERR_DUPLICATE_LABEL, name);
            return FALSE;
        }
    }
    
    /* Check table capacity */
    if (symtab->count >= MAX_LABELS) {
        print_error("Symbol table overflow", NULL);
        return FALSE;
    }
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int add_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    /* Validate before adding */
    if (!validate_symbol_addition(symtab, name, type))
        return FALSE;
    
    /* Add new symbol */
    strncpy(symtab->symbols[symtab->count].name, name, MAX_LABEL_LENGTH - 1);
    symtab->symbols[symtab->count].name[MAX_LABEL_LENGTH - 1] = NULL_TERMINATOR;
    symtab->symbols[symtab->count].value = value;
    symtab->symbols[symtab->count].type = type;
    symtab->symbols[symtab->count].is_entry = FALSE;
    symtab->symbols[symtab->count].is_extern = FALSE;
    symtab->count++;

    printf("DEBUG: Added symbol '%s' with value=%d, type=%d\n", name, value, type);
    
    return TRUE;
}

int find_symbol(const SymbolTable *symtab, const char *name) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0)
            return i;
    }
    return -1; /* Not found */
}

const Symbol* get_symbol(const SymbolTable *symtab, const char *name) {
    int index = find_symbol(symtab, name);

    return (index >= 0) ? &symtab->symbols[index] : NULL;
}

int process_label(char *label, SymbolTable *symtab, int address, int is_data) {
    char *colon = strchr(label, LABEL_TERMINATOR);
    
    /* Check if colon exists */
    if (!colon) {
        print_error(ERR_LABEL_MISSING_COLON, label);
        return FALSE;
    }

    *colon = NULL_TERMINATOR; /* Remove trailing colon */
    
    /* Check for empty label before colon */
    if (strlen(label) == 0) {
        print_error(ERR_LABEL_EMPTY, NULL);
        return FALSE;
    }
    
    if (!is_valid_label(label)) 
        return FALSE;
    
    return add_symbol(symtab, label, address, is_data ? DATA_SYMBOL : CODE_SYMBOL);
}

int has_entries(const SymbolTable *symtab) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) 
            return TRUE;
    }
    return FALSE;
}

int has_externs(const SymbolTable *symtab) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == EXTERNAL_SYMBOL) 
            return TRUE;
    }
    return FALSE;
}