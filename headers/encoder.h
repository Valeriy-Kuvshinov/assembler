#ifndef ENCODER_H
#define ENCODER_H

#include "memory.h"
#include "symbol_table.h"
#include "instructions.h"

#define BASE4_ENCODING 4

/* For register word bit positions */
#define REG_SRC_SHIFT 6  /* Source register starts at bit 6 */
#define REG_DST_SHIFT 2  /* Destination register starts at bit 2 */

/* Function prototypes */

int parse_operands(
    char **tokens, int token_count, int *operand_start_index, int *operand_count
);

int check_operands(const Instruction *inst, char **operands, int operand_count);

int encode_instruction_word(
    const Instruction *inst, MemoryImage *memory,
    int *current_ic_ptr, MemoryWord **current_word_ptr
);

int encode_operands(
    const Instruction *inst, char **operands, int operand_start,
    int operand_count, SymbolTable *symtab, MemoryImage *memory,
    int *current_ic_ptr, MemoryWord *instruction_word
);

void convert_to_base4_header(int value, char *result);

void convert_to_base4_address(int value, char *result);

void convert_to_base4_word(int value, char *result);

#endif