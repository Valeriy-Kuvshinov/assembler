#ifndef MEMORY_H
#define MEMORY_H

#include "utils.h"

/* Architecture limits */
#define MAX_WORD_COUNT 256         /* Total addressable memory (0-255) */
#define INSTRUCTION_START   100    /* First executable address (IC initial value) */
#define MAX_DC_SIZE   100          /* Available data words DC (0-99) */
#define MAX_IC_SIZE   156          /* Available instruction words IC (100-255) */

#define WORD_MASK 0x3FF            /* Mask to enforce 10-bit word size */

#define REGISTER_CHAR 'r'          /* r0-r7 */
#define PSW_REGISTER "PSW"         /* Program Status Word register */
#define MIN_REGISTER '0'           /* Min register index */
#define MAX_REGISTER '7'           /* Max register index */

#define ADDR_LENGTH 5              /* 4 + null terminator */
#define WORD_LENGTH 6              /* 5 + null terminator */

/* Memory word representation */
typedef struct {
    unsigned int value : 10;       /* 10-bit value */
    unsigned int are : 2;          /* A/R/E bits */
    int ext_symbol_index;          /* Index of external symbol in symbol table (-1 if not external) */
} MemoryWord;

/* Memory image */
typedef struct {
    MemoryWord words[MAX_WORD_COUNT];
    int ic;                        /* Instruction counter */
    int dc;                        /* Data counter */
} MemoryImage;

/* Validation macros */

#define IS_REGISTER(str) \
    ((str)[0] == REGISTER_CHAR && (str)[1] >= MIN_REGISTER && (str)[1] <= MAX_REGISTER && (str)[2] == NULL_TERMINATOR)

#define IS_PSW(str) \
    (strcmp((str), PSW_REGISTER) == 0)

#define IS_REGISTER_OR_PSW(str) \
    (IS_REGISTER(str) || IS_PSW(str))

#endif