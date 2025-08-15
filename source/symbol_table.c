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
/*
Function to dynamically expand the symbol table capacity when full.
Receives: SymbolTable *table - Table to resize
Returns: int - TRUE if resize succeeded, FALSE on memory error
*/
static int resize_symbol_table(SymbolTable *table) {
    int new_capacity;
    Symbol *new_symbols;

    new_capacity = (table->capacity == 0) ? INITIAL_SYMBOLS_CAPACITY : table->capacity * 2;
    new_symbols = realloc(table->symbols, new_capacity * sizeof(Symbol));

    if (!new_symbols) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize symbol table");
        return FALSE;
    }
    table->symbols = new_symbols;
    table->capacity = new_capacity;

    return TRUE;
}

/*
Function to validate label syntax.
Checks:
- Non-empty string
- Proper label terminator (:)
- Length within limits
- Starts with letter
- Contains only alphanumeric chars
Receives: const char *label - Label string to validate
Returns: int - TRUE if valid syntax, FALSE otherwise
*/
static int is_valid_syntax(const char *label) {
    int i;
    size_t length;

    if (!label || !*label) {
        print_error(ERR_LABEL_SYNTAX, NULL);
        return FALSE;
    }
    length = strlen(label);

    if (label[length - 1] != LABEL_TERMINATOR) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    if (length >= MAX_LABEL_NAME_LENGTH) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    if (!isalpha(label[0])) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    for (i = 1; i < length - 1; i++) {
        if (!isalnum(label[i])) {
            print_error(ERR_LABEL_SYNTAX, label);
            return FALSE;
        }
    }
    return TRUE;
}

/*
Function to check if label matches any reserved keywords.
Checks:
- Register names (r0-r7, PSW)
- Instruction names
- Directive names
- Macro keywords
Receives: const char *label - Label to check
Returns: int - TRUE if reserved word, FALSE otherwise
*/
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

/*
Function to store a new symbol in the table.
Receives: SymbolTable *table - Target symbol table
          const char *name - Symbol name
          int value - Associated numeric value
          int type - Symbol type (CODE/DATA/EXTERNAL/ENTRY)
*/
static void store_symbol(SymbolTable *table, const char *name, int value, int type) {
    char *dest_name;

    dest_name = table->symbols[table->count].name;
    bounded_string_copy(dest_name, name, MAX_LABEL_NAME_LENGTH, "symbol name storage");

    table->symbols[table->count].name[MAX_LABEL_NAME_LENGTH - 1] = NULL_TERMINATOR;
    table->symbols[table->count].value = value;
    table->symbols[table->count].type = type;
    table->symbols[table->count].is_entry = (type == ENTRY_SYMBOL);
    table->symbols[table->count].is_extern = (type == EXTERNAL_SYMBOL);

    table->count++;
}

/*
Function to check for conflicts when adding a symbol.
Detects:
- .extern vs .entry conflicts
- Redefinition attempts
Receives: const Symbol *existing_symbol - Found symbol or NULL
          int new_type - Proposed symbol type
          const char *name - Symbol name for error reporting
Returns: int - TRUE if conflict exists, FALSE otherwise
*/
static int check_symbol_conflict(const Symbol *existing_symbol, int new_type, const char *name) {
    if (!existing_symbol)
        return FALSE;

    if ((IS_EXTERNAL_SYMBOL(*existing_symbol) &&
        (new_type == ENTRY_SYMBOL)) ||
        (IS_ENTRY_SYMBOL(*existing_symbol) &&
        (new_type == EXTERNAL_SYMBOL))) {
        print_error("Label cannot be both .extern and .entry", name);
        return TRUE;
    }
    print_error("Label is already defined", name);

    return TRUE;
}

/*
Function to remove label terminator (:) from a label string.
Receives: char *label - Label to process (modified in-place)
*/
static void extract_text_from_label(char *label) {
    char *colon = strchr(label, LABEL_TERMINATOR);

    if (colon)
        *colon = NULL_TERMINATOR; /* Remove the colon */
}

/* Outer methods */
/* ==================================================================== */
/*
Function to initialize the symbol table with default capacity.
Receives: SymbolTable *table - Pointer to table structure to initialize
Returns: int - TRUE if initialization succeeded, FALSE on memory error
*/
int init_symbol_table(SymbolTable *table) {
    table->symbols = malloc(INITIAL_SYMBOLS_CAPACITY * sizeof(Symbol));

    if (!table->symbols) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to initialize symbol table");
        return FALSE;
    }
    table->count = 0;
    table->capacity = INITIAL_SYMBOLS_CAPACITY;

    return TRUE;
}

/*
Function to free all resources associated with the symbol table.
Receives: SymbolTable *table - Table to deallocate
*/
void free_symbol_table(SymbolTable *table) {
    safe_free((void**)&table->symbols);

    table->count = 0;
    table->capacity = 0;
}

/*
Function to locate a symbol by name in the table.
Receives: SymbolTable *table - Table to search
          const char *name - Symbol name to find
Returns: Symbol* - Pointer to found symbol or NULL
*/
Symbol* find_symbol(SymbolTable *table, const char *name) {
    int i;

    if (!table || !table->symbols || !name)
        return NULL;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0)
            return &table->symbols[i];
    }
    return NULL;
}

/*
Function to add a new symbol to the table.
Handles memory allocation, conflict checking, automatic resizing.
Receives: SymbolTable *table - Target symbol table
          const char *name - Symbol name
          int value - Numeric value
          int type - Symbol type
Returns: int - TRUE if added successfully, FALSE on error
*/
int add_symbol(SymbolTable *table, const char *name, int value, int type) {
    Symbol *existing_symbol;

    if (!table) {
        print_error("Symbol table not initialized", NULL);
        return FALSE;
    }
    existing_symbol = find_symbol(table, name);

    if (check_symbol_conflict(existing_symbol, type, name))
        return FALSE;

    if ((table->count >= table->capacity) &&
        (!resize_symbol_table(table)))
        return FALSE;

    store_symbol(table, name, value, type);

    return TRUE;
}

/*
Function to check if table contains any entry symbols.
Receives: const SymbolTable *table - Table to check
Returns: int - TRUE if at least one entry exists
*/
int has_entries(const SymbolTable *table) {
    int i;

    if (!table || !table->symbols)
        return FALSE;

    for (i = 0; i < table->count; i++) {
        if (IS_ENTRY_SYMBOL(table->symbols[i]))
            return TRUE;
    }
    return FALSE;
}

/*
Function to check if table contains any external symbols.
Receives: const SymbolTable *table - Table to check
Returns: int - TRUE if at least one extern exists
*/
int has_externs(const SymbolTable *table) {
    int i;

    if (!table || !table->symbols)
        return FALSE;

    for (i = 0; i < table->count; i++) {
        if (IS_EXTERNAL_SYMBOL(table->symbols[i]))
            return TRUE;
    }
    return FALSE;
}

/*
Function to perform complete label validation including syntax and reserved words.
Receives: char *label - Label to validate (modified in-place)
Returns: int - TRUE if valid label, FALSE otherwise
*/
int is_valid_label(char *label) {
    char temp[MAX_LABEL_NAME_LENGTH];

    if (!is_valid_syntax(label))
        return FALSE;

    bounded_string_copy(temp, label, MAX_LABEL_NAME_LENGTH, "label validation");
    extract_text_from_label(temp);

    if (is_reserved_word(temp))
        return FALSE;

    return TRUE;
}

/*
Function to check if first token in array is a label definition.
Receives: char **tokens - Array of tokens
          int token_count - Number of tokens in array
Returns: int - TRUE if first token contains label terminator, FALSE otherwise
*/
int is_first_token_label(char **tokens, int token_count) {
    return (token_count > 0 && strchr(tokens[0], LABEL_TERMINATOR));
}

/*
Function to process a label definition from source code.
Receives: char *label - Label string (with terminator)
          SymbolTable *table - Target symbol table
          int address - Memory address for symbol
          int symbol_type - Symbol type
Returns: int - TRUE if processed successfully, FALSE otherwise
*/
int process_label(char *label, SymbolTable *table, int address, int symbol_type) {
    extract_text_from_label(label);
    return add_symbol(table, label, address, symbol_type);
}

/*
Function to update the values of all data symbols by adding the final IC
This adjusts data symbol addresses to come after all instructions in memory
Receives: SymbolTable *symtab - Pointer to the symbol table
          int final_ic - The final instruction counter value
*/
void update_data_symbols(SymbolTable *symtab, int final_ic) {
    int i;

    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL)
            symtab->symbols[i].value += IC_START + final_ic;
    }
}