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
    int new_capacity = (symtab->capacity == 0) ? INITIAL_SYMBOLS_CAPACITY : symtab->capacity * 2;
    Symbol *new_symbols = realloc(symtab->symbols, new_capacity * sizeof(Symbol));
    
    if (!new_symbols) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize symbol table");
        return FALSE;
    }
    symtab->symbols = new_symbols;
    symtab->capacity = new_capacity;

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
    
    if (IS_DIRECTIVE(label)) {
        print_error("Label cannot be directive (.data / .string / .mat / .entry / .extern)", label);
        return TRUE;
    }
    
    if (IS_MACRO_KEYWORD(label)) {
        print_error("Label cannot be macro keyword (mcro / mcroend)", label);
        return TRUE;
    }
    return FALSE;
}

static void store_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    strncpy(symtab->symbols[symtab->count].name, name, MAX_LABEL_NAME_LENGTH - 1);

    symtab->symbols[symtab->count].name[MAX_LABEL_NAME_LENGTH - 1] = NULL_TERMINATOR;
    symtab->symbols[symtab->count].value = value;
    symtab->symbols[symtab->count].type = type;
    symtab->symbols[symtab->count].is_entry = (type == ENTRY_SYMBOL);
    symtab->symbols[symtab->count].is_extern = (type == EXTERNAL_SYMBOL);
    symtab->count++;
}

/* Outer regular methods */
/* ==================================================================== */
int init_symbol_table(SymbolTable *symtab) {
    symtab->symbols = malloc(INITIAL_SYMBOLS_CAPACITY * sizeof(Symbol));

    if (!symtab->symbols) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to initialize symbol table");
        return FALSE;
    }
    symtab->count = 0;
    symtab->capacity = INITIAL_SYMBOLS_CAPACITY;

    return TRUE;
}

void free_symbol_table(SymbolTable *symtab) {
    safe_free((void**)&symtab->symbols);

    symtab->count = 0;
    symtab->capacity = 0;
}

void extract_text_from_label(char *label) {
    char *colon = strchr(label, LABEL_TERMINATOR);

    if (colon)
        *colon = NULL_TERMINATOR; /* Remove the colon */
}

Symbol* find_symbol(SymbolTable *symtab, const char *name) {
    int i;

    if (!symtab || !symtab->symbols || !name)
        return NULL;

    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0)
            return &symtab->symbols[i];
    }
    return NULL;
}

int add_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    Symbol *existing_symbol;

    if (!symtab) {
        print_error("Symbol table not initialized", NULL);
        return FALSE;
    }

    existing_symbol = find_symbol(symtab, name);

    if (existing_symbol != NULL) {
        /* A symbol with this name already exists, check for conflicts */
        if ((existing_symbol->type == EXTERNAL_SYMBOL && type == ENTRY_SYMBOL) ||
            (existing_symbol->type == ENTRY_SYMBOL && type == EXTERNAL_SYMBOL)) {
            print_error("Label cannot be both .extern and .entry", name);
            return FALSE;
        }
        print_error("Label is already defined", name);
        return FALSE;
    }
    
    if (symtab->count >= symtab->capacity) {
        if (!resize_symbol_table(symtab))
            return FALSE;
    }
    store_symbol(symtab, name, value, type);
    
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

int is_valid_label(char *label) {
    char temp[MAX_LABEL_NAME_LENGTH];

    if (!is_valid_syntax(label))
        return FALSE;

    strncpy(temp, label, MAX_LABEL_NAME_LENGTH);
    temp[MAX_LABEL_NAME_LENGTH - 1] = NULL_TERMINATOR;
    
    extract_text_from_label(temp);

    if (is_reserved_word(temp))
        return FALSE;

    return TRUE;
}

int process_label(char *label, SymbolTable *symtab, int address, int is_data) {
    extract_text_from_label(label);
    return add_symbol(symtab, label, address, is_data ? DATA_SYMBOL : CODE_SYMBOL);
}