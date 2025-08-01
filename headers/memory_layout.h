#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

/* Memory Configuration */
#define WORD_COUNT 256 /* Total number of memory words - cells */
#define WORD_MASK 0x3FF /* Mask to enforce 10-bit word size */
#define IC_START 100 /* Instruction Base Address */

#define MAX_REGISTER 7 /* Maximum register index, r0 to r7 */

/* Symbol Table Configuration */
#define MAX_LABEL_LENGTH 31 /* Max label length (30 + null terminator) */
#define MAX_LABELS 1000 /* Max number of labels in symbol table */

#define CODE_SYMBOL 0
#define DATA_SYMBOL 1
#define EXTERNAL_SYMBOL 2
#define ENTRY_SYMBOL 3

/* A/R/E Field Values */
#define ARE_ABSOLUTE 0
#define ARE_RELOCATABLE 2
#define ARE_EXTERNAL 1

/* Base 4 encoding */
#define BASE4_DIGITS 4
#define BASE4_A_DIGIT 'a'
#define BASE4_B_DIGIT 'b'
#define BASE4_C_DIGIT 'c'
#define BASE4_D_DIGIT 'd'

#define ADDR_LENGTH 5 /* 4 + null terminator */
#define WORD_LENGTH 6 /* 5 + null terminator */

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

/* Memory word representation */
typedef struct {
    unsigned int value : 10;    /* 10-bit value */
    unsigned int are : 2;       /* A/R/E bits */
} MemoryWord;

/* Memory image */
typedef struct {
    MemoryWord words[WORD_COUNT];
    int ic;                     /* Instruction counter */
    int dc;                     /* Data counter */
} MemoryImage;

/* Function prototypes */

int has_entries(const SymbolTable *symtab);
int has_externs(const SymbolTable *symtab);

#endif