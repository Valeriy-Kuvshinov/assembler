#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#define MAX_INSTRUCTIONS 16

/* Instruction Word Layout */
#define OPCODE_SHIFT   6
#define SRC_SHIFT      4
#define DEST_SHIFT     2
#define ARE_SHIFT      0

#define OPCODE_BITS    4
#define SRC_ADDR_BITS  2
#define DEST_ADDR_BITS 2
#define ARE_BITS       2

#define MAX_OPERANDS   2
#define MAX_INSTRUCTION_LENGTH 5

/* Addressing Mode Numeric IDs (used when parsing operands) */
#define ADDR_MODE_IMMEDIATE 0
#define ADDR_MODE_DIRECT    1
#define ADDR_MODE_MATRIX    2
#define ADDR_MODE_REGISTER  3

/* Addressing Mode Legality Bit Flags */
#define ADDR_FLAG_IMMEDIATE (1 << 0)
#define ADDR_FLAG_DIRECT    (1 << 1)
#define ADDR_FLAG_MATRIX    (1 << 2)
#define ADDR_FLAG_REGISTER  (1 << 3)

typedef struct {
    /* the index is the opcode */
    const char *name;
    int num_operands;
    int legal_src_addr_modes;   /* bitmask */
    int legal_dest_addr_modes;  /* bitmask */
} Instruction;

/* Function prototypes */

int get_instruction_opcode(const char *name);
const Instruction* get_instruction(const char *name);

#endif