#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "label.h"

/* Symbol Table Configuration */
#define MAX_LABELS 1000 /* Max number of labels in symbol table */

/* Symbol Types */
#define CODE_SYMBOL 0
#define DATA_SYMBOL 1
#define EXTERNAL_SYMBOL 2
#define ENTRY_SYMBOL 3

/* Symbol table entry */
typedef struct {
    char name[MAX_LABEL_LENGTH];
    int value;          /* Address value */
    int type;           /* CODE_SYMBOL, DATA_SYMBOL, etc. */
    int is_entry;       /* Boolean flag */
    int is_extern;      /* Boolean flag */
} Symbol;

/* Symbol table */
typedef struct {
    Symbol symbols[MAX_LABELS];
    int count;
} SymbolTable;

/* Function prototypes */

int add_symbol(SymbolTable *symtab, const char *name, int value, int type);
int find_symbol(const SymbolTable *symtab, const char *name);
const Symbol* get_symbol(const SymbolTable *symtab, const char *name);
int has_entries(const SymbolTable *symtab);
int has_externs(const SymbolTable *symtab);
int process_label(char *label, SymbolTable *symtab, int address, int is_data);

#endif