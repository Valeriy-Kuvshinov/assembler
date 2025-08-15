#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "memory.h"
#include "instructions.h"

static const Instruction instruction_set[INSTRUCTIONS_COUNT] = {
  /* group I: 2 operands */
  {
    "mov", 0, TWO_OPERANDS,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "cmp", 1, TWO_OPERANDS,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "add", 2, TWO_OPERANDS,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "sub", 3, TWO_OPERANDS,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "lea", 4, TWO_OPERANDS,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },

  /* group II: 1 operand */
  {
    "clr", 5, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "not", 6, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "inc", 7, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "dec", 8, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "jmp", 9, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "bne", 10, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "jsr", 11, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "red", 12, ONE_OPERAND, 0,
    ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },
  {
    "prn", 13, ONE_OPERAND, 0,
    ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER
  },

  /* group III: 0 operands */
  {"rts", 14, NO_OPERANDS, 0, 0},
  {"stop", 15, NO_OPERANDS, 0, 0}
};

static int check_operand_count(const Instruction *inst, int operand_count) {
    if (operand_count != inst->num_operands) {
        printf("Invalid instruction operands amount (%d / %d)%c", inst->num_operands, operand_count, NEWLINE);
        return FALSE;
    }
    return TRUE;
}

static int check_instruction_limit(const Instruction *inst, int length) {
    if (length > MAX_INSTRUCTION_WORDS) {
        print_error("Instruction exceeds the 5-word limit", inst->name);
        return FALSE;
    }
    return TRUE;
}

static int get_operand_word_cost(int addressing_mode) {
    switch (addressing_mode) {
        case ADDR_MODE_IMMEDIATE:
            return 1;
        case ADDR_MODE_MATRIX:
            return 2;
        case ADDR_MODE_REGISTER:
            return 1;
        case ADDR_MODE_DIRECT:
            return 1;
        default:
            return 0;
    }
}

const Instruction* get_instruction(const char *name) {
    int i;

    for (i = 0; i < INSTRUCTIONS_COUNT; i++) {
        if (strcmp(name, instruction_set[i].name) == 0)
            return &instruction_set[i];
    }
    return NULL;
}

int get_addressing_mode(const char *operand) {
    if (!operand || !*operand) 
        return -1;

    if (operand[0] == IMMEDIATE_PREFIX) 
        return ADDR_MODE_IMMEDIATE;

    if (strchr(operand, LEFT_BRACKET)) 
        return ADDR_MODE_MATRIX;

    if (IS_REGISTER(operand)) 
        return ADDR_MODE_REGISTER;

    return ADDR_MODE_DIRECT;
}

int calculate_instruction_length(const Instruction *inst, char **operands, int operand_count) {
    int i, mode;
    int length = 1;

    if (!inst || !operands)
        return -1;
    
    if (!check_operand_count(inst, operand_count))
        return -1;

    for (i = 0; i < operand_count; i++) {
        mode = get_addressing_mode(operands[i]);

        if (mode == -1)
            return -1;
        
        length += get_operand_word_cost(mode);
    }

    if ((operand_count == 2) &&
        (get_addressing_mode(operands[0]) == ADDR_MODE_REGISTER) &&
        (get_addressing_mode(operands[1]) == ADDR_MODE_REGISTER))
        length--;  /* Two registers share one word */

    if (!check_instruction_limit(inst, length))
        return -1;

    return length;
}