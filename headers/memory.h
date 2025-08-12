#ifndef MEMORY_H
#define MEMORY_H

#include "utils.h"

/* Architecture limits */
#define MAX_WORD_COUNT 256            /* total addressable memory (0-255) */
#define IC_START   100                /* first executable address (IC initial value) */
#define MAX_DC_SIZE   100             /* available data words DC (0-99) */
#define MAX_IC_SIZE   156             /* available instruction words IC (100-255) */

#define WORD_MASK 0x3FF               /* Mask to enforce 10-bit word size (bits 0-9 set to 1) */

#define REGISTER_CHAR 'r'             /* r0-r7 */
#define PSW_REGISTER "PSW"            /* Program Status Word register */
#define MIN_REGISTER '0'              /* min register index */
#define MAX_REGISTER '7'              /* max register index */
#define REGISTER_LENGTH 2          

#define ADDR_LENGTH 5                 /* 4 + null terminator */
#define WORD_LENGTH 6                 /* 5 + null terminator */

/* A/R/E Field Values */
#define ARE_ABSOLUTE 0                /* 00 in binary */
#define ARE_EXTERNAL 1                /* 01 in binary */ 
#define ARE_RELOCATABLE 2             /* 10 in binary */

/* First instruction word format */
typedef struct {
    unsigned int are    : 2;          /* A/R/E bits 0–1 */
    unsigned int dest   : 2;          /* bits 2–3 */
    unsigned int src    : 2;          /* bits 4–5 */
    unsigned int opcode : 4;          /* bits 6–9 */
} InstrWord;

/* Operand word: address or immediate value */
typedef struct {
    unsigned int are : 2;             /* A/R/E bits 0–1 */
    signed int value : 8;             /* bits 2–9 */
    signed int ext_symbol_index;
} OperandWord;

/* Register-pair operand word */
typedef struct {
    unsigned int are     : 2;         /* A/R/E bits 0–1 */
    unsigned int reg_dst : 4;         /* bits 2–5 */
    unsigned int reg_src : 4;         /* bits 6–9 */
} RegWord;

/* Data word for .data/.string/.mat */
typedef struct {
    signed int value : 10;            /* bits 0–9 */
} DataWord;

/* Union to store any type of word */
typedef union {
    InstrWord   instr;
    OperandWord operand;
    RegWord     reg;
    DataWord    data;
    unsigned int raw;                 /* for direct output to file */
} MemoryWord;

/* Memory image: flat array of words */
typedef struct {
    MemoryWord words[MAX_WORD_COUNT]; /* addresses 0–255 */
    unsigned int ic;                  /* instruction counter */
    unsigned int dc;                  /* data counter */
} MemoryImage;

/* Function prototypes */

void init_memory(MemoryImage *memory);

int validate_ic_limit(int current_ic);

int validate_dc_limit(int current_dc);

int store_value(MemoryImage *memory, int value);

/* Validation macros */

#define IS_REGISTER(str) \
    ((str)[0] == REGISTER_CHAR && (str)[1] >= MIN_REGISTER && (str)[1] <= MAX_REGISTER && (str)[2] == NULL_TERMINATOR)

#define IS_PSW(str) \
    (strcmp((str), PSW_REGISTER) == 0)

#define IS_REGISTER_OR_PSW(str) \
    (IS_REGISTER(str) || IS_PSW(str))

#endif