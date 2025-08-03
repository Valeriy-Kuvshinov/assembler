#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"

int is_valid_label(const char *label) {
    int i;
    
    /* Basic validation */
    if (!label || !*label || strlen(label) >= MAX_LABEL_LENGTH)
        return FALSE;
    
    /* First character must be alphabetic */
    if (!isalpha((unsigned char)label[0]))
        return FALSE;
        
    /* Subsequent characters must be alphanumeric or underscore*/
    for (i = 1; label[i]; i++) {
        if (!isalnum((unsigned char)label[i]) && label[i] != UNDERSCORE_CHAR)
            return FALSE;
    }
    return TRUE;
}

int add_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    int i;
    
    /* Check for duplicates */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0) {
            print_error(ERR_DUPLICATE_LABEL, name);
            return FALSE;
        }
    }
    
    if (symtab->count >= MAX_LABELS) {
        print_error("Symbol table overflow", NULL);
        return FALSE;
    }
    
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

int process_label(char *label, SymbolTable *symtab, int address, int is_data) {
    char *colon = strchr(label, LABEL_TERMINATOR); /* Remove trailing colon */

    if (colon) 
        *colon = NULL_TERMINATOR;
    
    if (!is_valid_label(label)) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    return add_symbol(symtab, label, address, is_data ? DATA_SYMBOL : CODE_SYMBOL);
}

int find_symbol(const SymbolTable *symtab, const char *name) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0) {
            return i;
        }
    }
    return -1; /* Not found */
}

const Symbol* get_symbol(const SymbolTable *symtab, const char *name) {
    int index = find_symbol(symtab, name);
    return (index >= 0) ? &symtab->symbols[index] : NULL;
}

void init_symbol_table(SymbolTable *symtab) {
    symtab->count = 0;
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

void update_data_symbol_addresses(SymbolTable *symtab, int ic_final) {
    int i;
    
    /* Update data symbol addresses to come after code section */
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL) {
            symtab->symbols[i].value += 100 + ic_final; /* IC_START + final_IC */
        }
    }
}