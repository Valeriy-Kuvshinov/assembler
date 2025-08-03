#ifndef SYMBOLS_H
#define SYMBOLS_H

/* Common symbols in input files and more... */
#define COMMENT_CHAR ';'
#define DIRECTIVE_CHAR '.'
#define UNDERSCORE_CHAR '_'
#define LABEL_TERMINATOR ':'
#define IMMEDIATE_PREFIX '#'
#define LEFT_BRACKET_CHAR '['
#define RIGHT_BRACKET_CHAR ']'
#define REGISTER_CHAR 'r'

#define NULL_TERMINATOR '\0'

/* Symbol Table Configuration */
#define MAX_LABEL_LENGTH 31 /* Max label length (30 + null terminator) */
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
int is_valid_label(const char *label);
int add_symbol(SymbolTable *symtab, const char *name, int value, int type);
int process_label(char *label, SymbolTable *symtab, int address, int is_data);
int find_symbol(const SymbolTable *symtab, const char *name);
const Symbol* get_symbol(const SymbolTable *symtab, const char *name);
void init_symbol_table(SymbolTable *symtab);
int has_entries(const SymbolTable *symtab);
int has_externs(const SymbolTable *symtab);
void update_data_symbol_addresses(SymbolTable *symtab, int ic_final);

#endif