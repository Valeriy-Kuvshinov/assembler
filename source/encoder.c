#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbol_table.h"
#include "instructions.h"
#include "encoder.h"

static int encode_immediate_operand(const char *operand, MemoryWord *word) {
    char *endptr;
    long value = strtol(operand + 1, &endptr, BASE10_ENCODING); /* Skip '#' */

    if (*endptr != NULL_TERMINATOR || (value < -128 || value > 127)) {
        print_error("# value must be a decimal integer within the signed range (-128 to 127)", operand);
        return FALSE;
    }
    word->operand.value = value;
    word->operand.are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_register_operand(const char *operand, MemoryWord *word) {
    int reg = operand[1] - '0';
    
    if (reg < 0 || reg > (MAX_REGISTER - '0')) {
        print_error("Register is outside the natural number range (0 to 7)", operand);
        return FALSE;
    }
    word->operand.value = reg;
    word->operand.are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_symbol_operand(const char *operand, SymbolTable *symtab, MemoryWord *word) {
    Symbol *sym = find_symbol(symtab, operand);
    
    if (!sym) {
        print_error("Symbol not found", operand);
        return FALSE;
    }

    if (sym->type == EXTERNAL_SYMBOL) {
        word->operand.value = 0;
        word->operand.are = ARE_EXTERNAL;
        word->operand.ext_symbol_index = (int)(sym - symtab->symbols);
    } else {
        word->operand.value = sym->value & WORD_MASK;
        word->operand.are = ARE_RELOCATABLE;
        word->operand.ext_symbol_index = -1;
    }
    return TRUE;
}

static char* extract_between_brackets(char *start, char open_delim, char close_delim, char **out_substring) {
    char *open, *close;

    open = strchr(start, open_delim);

    if (!open) 
        return NULL;

    close = strchr(open, close_delim);

    if (!close) 
        return NULL;

    *close = NULL_TERMINATOR;
    *out_substring = open + 1;

    return close + 1;
}

static int extract_matrix_registers(char *temp, char **reg1_str, char **reg2_str) {
    char *pos;

    pos = extract_between_brackets(temp, LEFT_BRACKET, RIGHT_BRACKET, reg1_str);

    if (!pos) {
        print_error(ERR_INVALID_MATRIX, "Missing first bracket pair");
        return FALSE;
    }

    pos = extract_between_brackets(pos, LEFT_BRACKET, RIGHT_BRACKET, reg2_str);

    if (!pos) {
        print_error(ERR_INVALID_MATRIX, "Missing second bracket pair");
        return FALSE;
    }
    return TRUE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *reg1_str, *reg2_str;
    size_t length1, length2;

    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;

    if (!extract_matrix_registers(temp, &reg1_str, &reg2_str))
        return FALSE;

    length1 = strlen(reg1_str);
    length2 = strlen(reg2_str);

    if (length1 != REGISTER_LENGTH || reg1_str[0] != REGISTER_CHAR || !isdigit(reg1_str[1]) ||
        length2 != REGISTER_LENGTH || reg2_str[0] != REGISTER_CHAR || !isdigit(reg2_str[1])) {
        print_error("Invalid register pair", "Incorrect register syntax in matrix addressing");
        return FALSE;
    }

    *base_reg = reg1_str[1] - '0';
    *index_reg = reg2_str[1] - '0';

    if (*base_reg < 0 || *base_reg > (MAX_REGISTER - '0') ||
        *index_reg < 0 || *index_reg > (MAX_REGISTER - '0')) {
        print_error("Invalid register pair", "Register out of bounds (r0-r7)");
        return FALSE;
    }
    return TRUE;
}

static int encode_matrix_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, MemoryWord *next_word) {
    char label[MAX_LABEL_NAME_LENGTH];
    char *bracket = strchr(operand, LEFT_BRACKET);
    int base_reg, index_reg;

    if (!next_word) {
        print_error(ERR_INVALID_MATRIX, "Matrix addressing requires a second word for registers");
        return FALSE;
    }

    /* Extract label part (e.g., "M1" from "M1[r2][r7]") */
    if (!bracket || bracket - operand >= MAX_LABEL_NAME_LENGTH) {
        print_error(ERR_INVALID_MATRIX, "Invalid label part in matrix addressing");
        return FALSE;
    }
    strncpy(label, operand, bracket - operand);
    label[bracket - operand] = NULL_TERMINATOR;

    /* Parse matrix registers (e.g., r2, r7) */
    if (!parse_matrix_operand(operand, &base_reg, &index_reg))
        return FALSE;

    /* Encode the label part (first word of matrix operand) */
    if (!encode_symbol_operand(label, symtab, word))
        return FALSE;

    next_word->raw = 0; /* Clear bits before setting */
    next_word->reg.reg_src = base_reg; /* Assign to reg_src */
    next_word->reg.reg_dst = index_reg; /* Assign to reg_dst */
    next_word->reg.are = ARE_ABSOLUTE; /* Register word is always absolute */

    return TRUE;
}

static int encode_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, int is_dest, MemoryWord *next_word) {
    word->raw = 0;
    word->operand.ext_symbol_index = -1;

    if (next_word) {
        next_word->raw = 0;
        next_word->operand.ext_symbol_index = -1;
    }

    /* Immediate value (#num) */
    if (operand[0] == IMMEDIATE_PREFIX) {
        if (!encode_immediate_operand(operand, word)) 
            return FALSE;
    }
    /* Register (r0-r7) */
    else if (strlen(operand) == REGISTER_LENGTH && operand[0] == REGISTER_CHAR && isdigit(operand[1])) {
        if (!encode_register_operand(operand, word)) 
            return FALSE;
    }
    /* Matrix access (label[rX][rY]) */
    else if (strchr(operand, LEFT_BRACKET)) {
        if (!encode_matrix_operand(operand, symtab, word, next_word)) 
            return FALSE;
    }
    /* Symbol/label (direct addressing) */
    else if (!encode_symbol_operand(operand, symtab, word)) 
        return FALSE;

    return TRUE;
}

static int store_operand_word(MemoryImage *memory, int *current_ic_ptr, MemoryWord operand_word) {
    int addr_index;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    /* Store the word at the calculated instruction address */
    addr_index = IC_START + (*current_ic_ptr);
    memory->words[addr_index] = operand_word;
    (*current_ic_ptr)++;

    return TRUE;
}

static int store_register_word(MemoryImage *memory, int *current_ic_ptr, int reg_value, int shift) {
    MemoryWord *target_word;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    target_word = &memory->words[IC_START + (*current_ic_ptr)];
    target_word->raw = 0;

    if (shift == 6)
        target_word->reg.reg_src = reg_value; /* Source register */
    else if (shift == 2)
        target_word->reg.reg_dst = reg_value; /* Destination register */

    target_word->reg.are = ARE_ABSOLUTE;
    (*current_ic_ptr)++;

    return TRUE;
}

static int store_two_registers(MemoryImage *memory, int *current_ic_ptr, MemoryWord src_word, MemoryWord dest_word) {
    MemoryWord *target_word;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    target_word = &memory->words[IC_START + (*current_ic_ptr)];
    target_word->raw = 0;

    /* Combine source and destination register values into a single RegWord */
    target_word->reg.reg_src = src_word.operand.value;
    target_word->reg.reg_dst = dest_word.operand.value;
    target_word->reg.are = ARE_ABSOLUTE;    
    (*current_ic_ptr)++;

    return TRUE;
}

static int process_operand_storage(MemoryImage *memory, int *current_ic_ptr, int mode, MemoryWord operand_word, MemoryWord next_word, int reg_shift) {
    if (mode == ADDR_MODE_REGISTER)
        return store_register_word(memory, current_ic_ptr, operand_word.operand.value, reg_shift);
    else {
        /* Store the primary operand word (immediate, direct, or matrix label) */
        if (!store_operand_word(memory, current_ic_ptr, operand_word))
            return FALSE;

        /* If it's matrix addressing, store the second word (registers) */
        if (mode == ADDR_MODE_MATRIX)
            return store_operand_word(memory, current_ic_ptr, next_word);
    }
    return TRUE;
}

static int handle_one_operand_encoding(const Instruction *inst, char **operands, int operand_start, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    int src_mode;
    MemoryWord operand_word, next_operand_word; 
    operand_word.raw = 0;
    next_operand_word.raw = 0;

    /* Encode the single operand (which is always the destination) */
    if (!encode_operand(operands[operand_start], symtab, &operand_word, FALSE, &next_operand_word))
        return FALSE;

    src_mode = get_addressing_mode(operands[operand_start]);

    instruction_word->instr.dest = src_mode;
    instruction_word->instr.src = 0;

    return process_operand_storage(memory, current_ic_ptr, src_mode, operand_word, next_operand_word, 2);
}

static int handle_two_operand_encoding(const Instruction *inst, char **operands, int operand_start, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    int src_mode, dest_mode;
    MemoryWord src_operand_word, src_next_operand_word, dest_operand_word, dest_next_operand_word; 
    src_operand_word.raw = 0;
    src_next_operand_word.raw = 0;
    dest_operand_word.raw = 0;
    dest_next_operand_word.raw = 0;

    /* Encode source operand */
    if (!encode_operand(operands[operand_start], symtab, &src_operand_word, FALSE, &src_next_operand_word))
        return FALSE;

    src_mode = get_addressing_mode(operands[operand_start]);
    instruction_word->instr.src = src_mode;

    /* Encode destination operand */
    if (!encode_operand(operands[operand_start + 1], symtab, &dest_operand_word, TRUE, &dest_next_operand_word))
        return FALSE;

    dest_mode = get_addressing_mode(operands[operand_start + 1]);
    instruction_word->instr.dest = dest_mode; /* Set destination addressing mode in instruction word */

    /* Two-register optimization: if both source and destination are registers, they share one word */
    if (src_mode == ADDR_MODE_REGISTER && dest_mode == ADDR_MODE_REGISTER)
        return store_two_registers(memory, current_ic_ptr, src_operand_word, dest_operand_word);
    else {
        /* Store source operand */
        if (!process_operand_storage(memory, current_ic_ptr, src_mode, src_operand_word, src_next_operand_word, 6))
            return FALSE;

        /* Store destination operand */
        return process_operand_storage(memory, current_ic_ptr, dest_mode, dest_operand_word, dest_next_operand_word, 2);
    }
}

static int encode_instruction_operands(const Instruction *inst, char **operands, int operand_start, int operand_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    if (inst->num_operands == NO_OPERANDS) 
        return TRUE;

    if (inst->num_operands == ONE_OPERAND)
        return handle_one_operand_encoding(inst, operands, operand_start, symtab, memory, current_ic_ptr, instruction_word);
    else if (inst->num_operands == TWO_OPERANDS)
        return handle_two_operand_encoding(inst, operands, operand_start, symtab, memory, current_ic_ptr, instruction_word);

    return FALSE;
}

static int encode_instruction_word(const Instruction *inst, MemoryImage *memory, int *current_ic_ptr, MemoryWord **current_word_ptr) {
    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    *current_word_ptr = &memory->words[IC_START + *current_ic_ptr];
    (*current_ic_ptr)++;

    (*current_word_ptr)->raw = 0; 
    (*current_word_ptr)->instr.opcode = inst->opcode;
    (*current_word_ptr)->instr.are = ARE_ABSOLUTE;

    return TRUE;
}

static int parse_instruction_operands(char **tokens, int token_count, int *operand_start_index, int *operand_count) {
    int i;
    *operand_start_index = 0;

    if (has_label_in_tokens(tokens, token_count))
        *operand_start_index = 2; /* Skip label and instruction name */
    else
        *operand_start_index = 1; /* Skip only instruction name */

    *operand_count = 0;

    for (i = *operand_start_index; i < token_count; i++) {
        (*operand_count)++;
    }
    return TRUE;
}

static int check_operands(const Instruction *inst, char **operands, int operand_count) {
    int i;

    for (i = 0; i < operand_count; i++) {
        int addr_mode_flag, legal_modes;

        addr_mode_flag = get_addressing_mode(operands[i]);

        /* Determine the legal addressing modes based on operand position (source/destination) */
        switch (inst->num_operands) {
            case TWO_OPERANDS:
                legal_modes = (i == 0) ? inst->legal_src_addr_modes : inst->legal_dest_addr_modes;
                break;
            case ONE_OPERAND:
                legal_modes = inst->legal_dest_addr_modes;
                break;
            case NO_OPERANDS:
                legal_modes = 0; /* No operands, so no legal modes */
                break;
            default:
                return FALSE;
        }
        /* Check if the operand's addressing mode is allowed */
        if (!(legal_modes & (1 << addr_mode_flag))) {
            print_error("Invalid addressing mode for instruction operand", operands[i]);
            return FALSE;
        }
    }
    return TRUE;
}

int encode_instruction(const Instruction *inst, char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr) {
    int operand_start_index, operand_count;
    MemoryWord *instruction_word;

    /* Parse operands to get their starting index and count */
    if (!parse_instruction_operands(tokens, token_count, &operand_start_index, &operand_count))
        return FALSE;

    /* Validate addressing modes for the instruction's operands */
    if (!check_operands(inst, tokens + operand_start_index, operand_count)) 
        return FALSE;

    /* Encode the first word of the instruction (opcode, ARE, addressing modes) */
    if (!encode_instruction_word(inst, memory, current_ic_ptr, &instruction_word))
        return FALSE;

    /* Encode the subsequent operand words */
    return encode_instruction_operands(inst, tokens, operand_start_index, operand_count, symtab, memory, current_ic_ptr, instruction_word);
}