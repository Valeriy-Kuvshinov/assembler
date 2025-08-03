#ifndef MEMORY_H
#define MEMORY_H

/* Memory Configuration */
#define WORD_COUNT 256 /* Total number of memory words (cells), 0 - 255 */
#define WORD_MASK 0x3FF /* Mask to enforce 10-bit word size */
#define IC_START 100 /* Instruction Base Address */

#define MAX_REGISTER 7 /* Maximum register index, r0 to r7 */

#define ADDR_LENGTH 5 /* 4 + null terminator */
#define WORD_LENGTH 6 /* 5 + null terminator */

/* Memory word representation */
typedef struct {
    unsigned int value : 10;    /* 10-bit value */
    unsigned int are : 2;       /* A/R/E bits */
    int ext_symbol_index;       /* Index of external symbol in symbol table (-1 if not external) */
} MemoryWord;

/* Memory image */
typedef struct {
    MemoryWord words[WORD_COUNT];
    int ic;                     /* Instruction counter */
    int dc;                     /* Data counter */
} MemoryImage;

#endif