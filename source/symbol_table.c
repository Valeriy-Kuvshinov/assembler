#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "instructions.h"
#include "directives.h"
#include "macro_table.h"
#include "symbol_table.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int resize_symbol_table(SymbolTable *symtab) {
    int new_capacity = (symtab->capacity == 0) ? 4 : symtab->capacity * 2;
    Symbol *new_symbols = realloc(symtab->symbols, new_capacity * sizeof(Symbol));
    
    if (!new_symbols) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize symbol table");
        return FALSE;
    }
    
    symtab->symbols = new_symbols;
    symtab->capacity = new_capacity;

    return TRUE;
}

static int validate_symbol_addition(const SymbolTable *symtab, const char *symbol, int type) {
    int i;

    /* Check for NULL table */
    if (!symtab) {
        print_error("Symbol table not initialized", NULL);
        return FALSE;
    }

    /* Check for duplicates and conflicts */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, symbol) == 0) {
            if ((symtab->symbols[i].type == EXTERNAL_SYMBOL && type == ENTRY_SYMBOL) ||
                (symtab->symbols[i].type == ENTRY_SYMBOL && type == EXTERNAL_SYMBOL)) {
                print_error("Label cannot be both .extern and .entry", symbol);
                return FALSE;
            }
            print_error("Label is already defined", symbol);
            return FALSE;
        }
    }
    return TRUE;
}

static int is_valid_syntax(const char *label) {
    size_t len;
    int i;
    
    if (!label || !*label) {
        print_error(ERR_LABEL_SYNTAX, NULL);
        return FALSE;
    }
    
    len = strlen(label);
    
    if (label[len-1] != LABEL_TERMINATOR) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    if (len >= MAX_LABEL_NAME_LENGTH) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    if (!isalpha(label[0])) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    for (i = 1; i < len-1; i++) {
        if (!isalnum(label[i])) {
            print_error(ERR_LABEL_SYNTAX, label);
            return FALSE;
        }
    }
    return TRUE;
}

static int is_reserved_word(const char *label) {
    if (IS_REGISTER_OR_PSW(label)) {
        print_error("Label cannot be register (r0 - r7 / PSW)", label);
        return TRUE;
    }

    if (IS_INSTRUCTION(label)) {
        print_error("Label cannot be instruction (mov / cmp / add / ...)", label);
        return TRUE;
    }
    
    if (IS_DATA_DIRECTIVE(label) || IS_LINKER_DIRECTIVE(label)) {
        print_error("Label cannot be directive (.data / .string / .mat / .entry / .extern)", label);
        return TRUE;
    }
    
    if (IS_MACRO_KEYWORD(label)) {
        print_error("Label cannot be macro keyword (mcro / mcroend)", label);
        return TRUE;
    }
    return FALSE;
}

/* Outer regular methods */
/* ==================================================================== */
int init_symbol_table(SymbolTable *symtab) {
    symtab->symbols = NULL;
    symtab->count = 0;
    symtab->capacity = 0;

    return TRUE;
}

void free_symbol_table(SymbolTable *symtab) {
    if (symtab->symbols) {
        free(symtab->symbols);
        symtab->symbols = NULL;
    }
    symtab->count = 0;
    symtab->capacity = 0;
}

int add_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    if (!validate_symbol_addition(symtab, name, type))
        return FALSE;
    
    if (symtab->count >= symtab->capacity) {
        if (!resize_symbol_table(symtab))
            return FALSE;
    }
    
    /* Add new symbol */
    strncpy(symtab->symbols[symtab->count].name, name, MAX_LABEL_NAME_LENGTH - 1);
    symtab->symbols[symtab->count].name[MAX_LABEL_NAME_LENGTH - 1] = '\0';
    symtab->symbols[symtab->count].value = value;
    symtab->symbols[symtab->count].type = type;
    symtab->symbols[symtab->count].is_entry = (type == ENTRY_SYMBOL);
    symtab->symbols[symtab->count].is_extern = (type == EXTERNAL_SYMBOL);
    symtab->count++;
    
    return TRUE;
}

int has_entries(const SymbolTable *symtab) {
    int i;
    
    if (!symtab || !symtab->symbols)
        return FALSE;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) 
            return TRUE;
    }
    return FALSE;
}

int has_externs(const SymbolTable *symtab) {
    int i;
    
    if (!symtab || !symtab->symbols)
        return FALSE;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == EXTERNAL_SYMBOL) 
            return TRUE;
    }
    return FALSE;
}

int is_valid_label(const char *label) {
    char name_only[MAX_LABEL_NAME_LENGTH];
    size_t len;
    
    if (!is_valid_syntax(label))
        return FALSE;
    
    /* Extract name portion (without colon) */
    len = strlen(label);
    strncpy(name_only, label, len-1);
    name_only[len-1] = NULL_TERMINATOR;
    
    if (is_reserved_word(name_only))
        return FALSE;
    
    return TRUE;
}

int process_label(char *label, SymbolTable *symtab, int address, int is_data) {
    char *colon_pos;
    
    if (!is_valid_label(label))
        return FALSE;
    
    colon_pos = strchr(label, LABEL_TERMINATOR);
    *colon_pos = NULL_TERMINATOR;
    
    return add_symbol(symtab, label, address, is_data ? DATA_SYMBOL : CODE_SYMBOL);
}