#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "memory.h"
#include "instructions.h"

static const Instruction instruction_set[INSTRUCTIONS_COUNT] = {
  /* Group 0: Two operands */
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

  /* Group 1: One operand */
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

  /* Group 2: No operands */
  {"rts", 14, NO_OPERANDS, 0, 0},
  {"stop", 15, NO_OPERANDS, 0, 0}
};

const Instruction* get_instruction(const char *name) {
    int i;

    for (i = 0; i < INSTRUCTIONS_COUNT; i++) {
        if (strcmp(name, instruction_set[i].name) == 0)
            return &instruction_set[i];
    }
    return NULL;
}

int calculate_instruction_length(const Instruction *inst, char **operands, int operand_count) {
    int i;
    int length = 1; /* Base instruction word */

    if (operand_count > MAX_INSTRUCTION_WORDS) {
        print_error("Instruction exceeds 5-word limit", inst->name);
        return -1;
    }

    for (i = 0; i < operand_count; i++) {
        if (operands[i][0] == IMMEDIATE_PREFIX)
            length++;
        else if (strchr(operands[i], LEFT_BRACKET))
            length += 2;
        else if (operands[i][0] == REGISTER_CHAR &&
                 isdigit(operands[i][1]) && operands[i][2] == NULL_TERMINATOR)
            length++;
        else
            length++;
    }

    /* If instruction has 2 register operands, they can share 1 word */
    if (operand_count == 2) {
        int reg1 = (operands[0][0] == REGISTER_CHAR &&
                     isdigit(operands[0][1]) && operands[0][2] == NULL_TERMINATOR);
        int reg2 = (operands[1][0] == REGISTER_CHAR &&
                    isdigit(operands[1][1]) && operands[1][2] == NULL_TERMINATOR);

        if (reg1 && reg2)
            length--;
    }
    return length;
}