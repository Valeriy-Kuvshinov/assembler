#ifndef ENCODER_H
#define ENCODER_H

#include "memory.h"
#include "symbol_table.h"
#include "instructions.h"

#define BASE4_ENCODING 4

/* Function prototypes */

int encode_instruction(
    const Instruction *inst, char **tokens, int token_count, 
    SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr
);

void convert_to_base4_header(int value, char *result);

void convert_to_base4_address(int value, char *result);

void convert_to_base4_word(int value, char *result);

#endif