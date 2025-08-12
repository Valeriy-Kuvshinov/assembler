#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define INITIAL_SYMBOLS_CAPACITY 8

#define MAX_LABEL_NAME_LENGTH 31 /* max label name length (30 + null terminator) */

/* Symbol Types */
#define CODE_SYMBOL 0
#define DATA_SYMBOL 1
#define EXTERNAL_SYMBOL 2
#define ENTRY_SYMBOL 3

typedef struct {
    char name[MAX_LABEL_NAME_LENGTH];
    int value;                   /* address value */
    int type;                    /* CODE_SYMBOL, DATA_SYMBOL, etc. */
    int is_entry;                /* 'boolean' flag */
    int is_extern;               /* 'boolean' flag */
} Symbol;

typedef struct {
    Symbol *symbols;             /* dynamic array of symbols */
    unsigned int count;          /* current number of symbols */
    unsigned int capacity;       /* current capacity of the array */
} SymbolTable;

/* Function prototypes */

int init_symbol_table(SymbolTable *symtab);

void free_symbol_table(SymbolTable *symtab);

void extract_text_from_label(char *label);

Symbol* find_symbol(SymbolTable *symtab, const char *name);

int add_symbol(SymbolTable *symtab, const char *name, int value, int type);

int has_entries(const SymbolTable *symtab);

int has_externs(const SymbolTable *symtab);

int is_valid_label(char *label);

int process_label(char *label, SymbolTable *symtab, int address, int is_data);

#endif