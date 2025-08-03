#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "symbols.h"
#include "instructions.h"

const Instruction instruction_set[INSTRUCTIONS_COUNT] = {
    /* Group 0: Two operands */
    {"mov",  TWO_OPERANDS, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"cmp",  TWO_OPERANDS, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"add",  TWO_OPERANDS, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"sub",  TWO_OPERANDS, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"lea",  TWO_OPERANDS, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX},
    
    /* Group 1: One operand */
    {"clr",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"not",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"inc",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"dec",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"jmp",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"bne",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"jsr",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"red",  ONE_OPERAND, 0, ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    {"prn",  ONE_OPERAND, 0, ADDR_FLAG_IMMEDIATE | ADDR_FLAG_DIRECT | ADDR_FLAG_MATRIX | ADDR_FLAG_REGISTER},
    
    /* Group 2: No operands */
    {"rts",  NO_OPERANDS, 0, 0},
    {"stop", NO_OPERANDS, 0, 0}
};

const Instruction* get_instruction(const char *name) {
    int i;

    for (i = 0; i < INSTRUCTIONS_COUNT; i++) {
        if (strcmp(name, instruction_set[i].name) == 0)
            return &instruction_set[i];
    }
    return NULL;
}

int get_instruction_opcode(const char *name) {
    int i;

    for (i = 0; i < INSTRUCTIONS_COUNT; i++) {
        if (strcmp(name, instruction_set[i].name) == 0)
            return i;
    }
    return -1; /* Not found */
}

int calc_instruction_length(const Instruction *inst, char **operands, int operand_count) {
    int i;
    int length = 1; /* Base instruction word */
    
    for (i = 0; i < operand_count; i++) {
        if (operands[i][0] == IMMEDIATE_PREFIX) {
            /* Immediate - needs extra word */
            length++;
        }
        else if (strchr(operands[i], MATRIX_LEFT_BRACKET)) {
            /* Matrix - needs 2 extra words */
            length += 2;
        }
        else if (operands[i][0] == REGISTER_CHAR && isdigit(operands[i][1]) && operands[i][2] == NULL_TERMINATOR) {
            /* Register - based on your example, sub r1,r4 = 2 words total */
            /* So registers still need extra words in your implementation */
            length++;
        }
        else {
            /* Direct addressing - needs extra word */
            length++;
        }
    }
    
    /* Special case: if instruction has 2 register operands, they can share 1 word */
    if (operand_count == 2) {
        int reg1 = (operands[0][0] == REGISTER_CHAR && isdigit(operands[0][1]) && operands[0][2] == NULL_TERMINATOR);
        int reg2 = (operands[1][0] == REGISTER_CHAR && isdigit(operands[1][1]) && operands[1][2] == NULL_TERMINATOR);
        
        if (reg1 && reg2) {
            length--; /* Both registers share one word, so subtract 1 */
        }
    }
    return length;
}