#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#define INSTRUCTIONS_COUNT 16

/* Instruction Operands Amount */
#define TWO_OPERANDS 2          /* mov, cmp, add, sub, lea */
#define ONE_OPERAND  1          /* clr, not, inc, dec, jmp, bne, jsr, red, prn */
#define NO_OPERANDS  0          /* rts, stop */

#define MAX_INSTRUCTION_WORDS 5 /* maximum amount an instruction can require in memory. */

/* Addressing Mode Numeric IDs */
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
    const char *name;
    int opcode;
    int num_operands;           /* TWO_OPERANDS / ONE_OPERAND / NO_OPERANDS */
    int legal_src_addr_modes;   /* address mode for source operand */
    int legal_dest_addr_modes;  /* address mode for destination operand */
} Instruction;

/* Function prototypes */

const Instruction* get_instruction(const char *name);

int get_addressing_mode(const char *operand);

int calculate_instruction_length(const Instruction *inst, char **operands, int operand_count);

/* Validation macros */

#define IS_INSTRUCTION(name) \
    (get_instruction(name) != NULL)

#endif