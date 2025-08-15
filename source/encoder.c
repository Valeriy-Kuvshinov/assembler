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

/* Inner STATIC methods */
/* ==================================================================== */
/*
Function to reset all bits in a memory word to 0.
Receives: MemoryWord *word - Pointer to memory word to clear
*/
static void clear_bits(MemoryWord *word) {
    word->raw = 0;
}

/*
Function to skip immediate prefix & encode an immediate operand value.
Validates range (-128 to 127) and stores in memory word.
Receives: const char *operand - The operand string starting with '#'
          MemoryWord *word - Pointer to memory word for storage
Returns: int - TRUE if encoding succeeded, FALSE on invalid value
*/
static int encode_immediate_operand(const char *operand, MemoryWord *word) {
    char *endptr;
    long value = strtol(operand + 1, &endptr, BASE10_ENCODING);

    if (*endptr != NULL_TERMINATOR || (value < -128 || value > 127)) {
        print_error("# value must be a decimal integer within the signed range (-128 to 127)", operand);
        return FALSE;
    }
    word->operand.value = value;
    word->operand.are = ARE_ABSOLUTE;

    return TRUE;
}

/*
Function to encode a register operand, validates register number and stores in memory word.
Receives: const char *operand - The register operand string
          MemoryWord *word - Pointer to memory word for storage
Returns: int - TRUE if encoding succeeded, FALSE on invalid register
*/
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

/*
Function to encode a symbol / label operand, handles both external and relocatable symbols.
Receives: const char *operand - The symbol name
          SymbolTable *symtab - Pointer to symbol table
          MemoryWord *word - Pointer to memory word for storage
Returns: int - TRUE if encoding succeeded, FALSE if symbol not found
*/
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

/*
Function to extract content between brackets in matrix operand.
Receives: char *start - String starting at opening bracket
          char open_bracket - Opening bracket character
          char close_bracket - Closing bracket character
          char **out_str - Output pointer to content between brackets
Returns: char* - Pointer to position after closing bracket
*/
static char* extract_matrix_content(char *start, char open_bracket, char close_bracket, char **out_str) {
    char *open_pos, *close_pos;
    
    if (!start || !out_str)
        return NULL;

    open_pos = strchr(start, open_bracket);

    if (!open_pos)
        return NULL;

    close_pos = strchr(open_pos + 1, close_bracket);

    if (!close_pos)
        return NULL;

    if (close_pos == open_pos + 1)
        *out_str = NULL;  /* Empty content between brackets */
    else
        *out_str = open_pos + 1;  /* Skip opening bracket */

    *close_pos = NULL_TERMINATOR;
    
    return close_pos + 1;
}

/*
Function to extract both register indices from matrix operand.
Receives: char *temp - Buffer containing matrix operand
          char **reg1_str - Output for first register string
          char **reg2_str - Output for second register string
Returns: int - TRUE if both registers extracted successfully
*/
static int extract_matrix_registers(char *temp, char **reg1_str, char **reg2_str) {
    char *pos;

    if (!temp || !reg1_str || !reg2_str) {
        print_error(ERR_INVALID_MATRIX, "Null pointer in matrix register extraction");
        return FALSE;
    }
    pos = extract_matrix_content(temp, LEFT_BRACKET, RIGHT_BRACKET, reg1_str);

    if (!pos || !*reg1_str) {
        print_error(ERR_INVALID_MATRIX, "Missing or empty first matrix register");
        return FALSE;
    }
    pos = extract_matrix_content(pos, LEFT_BRACKET, RIGHT_BRACKET, reg2_str);

    if (!pos || !*reg2_str) {
        print_error(ERR_INVALID_MATRIX, "Missing or empty second matrix register");
        return FALSE;
    }
    return TRUE;
}

/*
Function to parse matrix operand into register numbers, validates register indices.
Receives: const char *operand - Full matrix operand string
          int *base_reg - Output for base register index
          int *index_reg - Output for index register index
Returns: int - TRUE if parsing succeeded, FALSE otherwise
*/
static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *reg1_str, *reg2_str;

    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;

    if (!extract_matrix_registers(temp, &reg1_str, &reg2_str))
        return FALSE;

    if (!IS_REGISTER(reg1_str) || !IS_REGISTER(reg2_str)) {
        print_error("Invalid register pair", "Matrix indices must be registers (r0-r7)");
        return FALSE;
    }
    *base_reg = reg1_str[1] - '0';
    *index_reg = reg2_str[1] - '0';

    if ((*base_reg < 0 || *base_reg > (MAX_REGISTER - '0')) ||
        (*index_reg < 0 || *index_reg > (MAX_REGISTER - '0'))) {
        print_error("Invalid register pair", "Register out of bounds (r0-r7)");
        return FALSE;
    }
    return TRUE;
}

/*
Function to encode a matrix operand, uses 2 memory words: 1 for label, 1 for registers.
Receives: const char *operand - Full matrix operand string
          SymbolTable *symtab - Pointer to symbol table
          MemoryWord *word - First memory word for label
          MemoryWord *next_word - Second memory word for registers
Returns: int - TRUE if encoding succeeded, FALSE otherwise
*/
static int encode_matrix_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, MemoryWord *next_word) {
    char label[MAX_LABEL_NAME_LENGTH];
    char *bracket = strchr(operand, LEFT_BRACKET);
    int base_reg, index_reg;

    if (!next_word) {
        print_error(ERR_INVALID_MATRIX, "Matrix addressing requires a second word for registers");
        return FALSE;
    }
    /* Extract label part (e.g., "M1" from "M1[r2][r7]") */
    if (!bracket || (bracket - operand >= MAX_LABEL_NAME_LENGTH)) {
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

    clear_bits(next_word);
    next_word->reg.reg_src = base_reg;
    next_word->reg.reg_dst = index_reg;
    next_word->reg.are = ARE_ABSOLUTE; /* Register word is always absolute */

    return TRUE;
}

/*
Function to route to specific encoder based on operand type.
Handles immediate, register, matrix, and symbol operands.
Receives: const char *operand - The operand string to encode
          SymbolTable *symtab - Pointer to symbol table
          MemoryWord *word - Primary memory word for storage
          int is_dest - Flag indicating if this is a destination operand
          MemoryWord *next_word - Secondary memory word (for matrix)
Returns: int - TRUE if encoding succeeded, FALSE otherwise
*/
static int encode_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, int is_dest, MemoryWord *next_word) {
    clear_bits(word);
    word->operand.ext_symbol_index = -1;

    if (next_word) {
        clear_bits(next_word);
        next_word->operand.ext_symbol_index = -1;
    }

    if (operand[0] == IMMEDIATE_PREFIX) { /* Immediate value (#num) */
        if (!encode_immediate_operand(operand, word)) 
            return FALSE;
    }
    else if (IS_REGISTER(operand)) { /* Register (r0-r7) */
        if (!encode_register_operand(operand, word)) 
            return FALSE;
    }
    else if (strchr(operand, LEFT_BRACKET)) { /* Matrix access (label[rX][rY]) */
        if (!encode_matrix_operand(operand, symtab, word, next_word)) 
            return FALSE;
    }
    else if (!encode_symbol_operand(operand, symtab, word)) /* Symbol/label (direct addressing) */
        return FALSE;

    return TRUE;
}

/*
Function to store an operand word in memory at current IC position.
Increments instruction counter after storage.
Receives: MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord operand_word - The word to store
Returns: int - TRUE if storage succeeded, FALSE on IC limit
*/
static int store_operand_word(MemoryImage *memory, int *current_ic_ptr, MemoryWord operand_word) {
    int addr_index;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    addr_index = IC_START + (*current_ic_ptr);
    memory->words[addr_index] = operand_word;
    (*current_ic_ptr)++;

    return TRUE;
}

/*
Function to store a register value in specific bits of a memory word.
Used for register-only instructions.
Receives: MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          int reg_value - Register number to store
          int shift - Bit position shift (src=6, dest=2)
Returns: int - TRUE if storage succeeded, FALSE on IC limit
*/
static int store_register_word(MemoryImage *memory, int *current_ic_ptr, int reg_value, int shift) {
    MemoryWord *target_word;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    target_word = &memory->words[IC_START + (*current_ic_ptr)];
    clear_bits(target_word);

    if (shift == REG_SRC_SHIFT)
        target_word->reg.reg_src = reg_value;
    else if (shift == REG_DST_SHIFT)
        target_word->reg.reg_dst = reg_value;

    target_word->reg.are = ARE_ABSOLUTE;
    (*current_ic_ptr)++;

    return TRUE;
}

/*
Function to store 2 register values in a single memory word.
Combine source and destination register values into a single word.
Receives: MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord src_word - Source register word
          MemoryWord dest_word - Destination register word
Returns: int - TRUE if storage succeeded, FALSE on IC limit
*/
static int store_two_registers(MemoryImage *memory, int *current_ic_ptr, MemoryWord src_word, MemoryWord dest_word) {
    MemoryWord *target_word;

    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    target_word = &memory->words[IC_START + (*current_ic_ptr)];
    clear_bits(target_word);

    target_word->reg.reg_src = src_word.operand.value;
    target_word->reg.reg_dst = dest_word.operand.value;
    target_word->reg.are = ARE_ABSOLUTE;    
    (*current_ic_ptr)++;

    return TRUE;
}

/*
Function to handle final storage of operand based on addressing mode.
Routes to appropriate storage function (primary word / second word).
Receives: MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          int mode - Addressing mode (immediate/register/matrix)
          MemoryWord operand_word - Primary operand word
          MemoryWord next_word - Secondary word (for matrix)
          int reg_shift - Bit shift for register storage
Returns: int - TRUE if storage succeeded, FALSE otherwise
*/
static int process_operand_storage(MemoryImage *memory, int *current_ic_ptr, int mode, MemoryWord operand_word, MemoryWord next_word, int reg_shift) {
    if (mode == ADDR_MODE_REGISTER)
        return store_register_word(memory, current_ic_ptr, operand_word.operand.value, reg_shift);
    else {
        if (!store_operand_word(memory, current_ic_ptr, operand_word))
            return FALSE;

        if (mode == ADDR_MODE_MATRIX)
            return store_operand_word(memory, current_ic_ptr, next_word);
    }
    return TRUE;
}

/*
Function to encode a single operand instruction.
Receives: const Instruction *inst - Instruction definition
          char **operands - Array of operand strings
          int operand_start - Index of first operand
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord *instruction_word - Pointer to instruction word
Returns: int - TRUE if encoding succeeded, FALSE otherwise
*/
static int encode_one_operand(const Instruction *inst, char **operands, int operand_start, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    int src_mode;
    MemoryWord operand_word, next_operand_word;
    operand_word.raw = 0;
    next_operand_word.raw = 0;

    if (!encode_operand(operands[operand_start], symtab, &operand_word, FALSE, &next_operand_word))
        return FALSE;

    src_mode = get_addressing_mode(operands[operand_start]);

    instruction_word->instr.dest = src_mode;
    instruction_word->instr.src = 0;

    return process_operand_storage(memory, current_ic_ptr, src_mode, operand_word, next_operand_word, 2);
}

/*
Function to encode a 2 operand instruction, handles special case of two registers in one word.
Receives: const Instruction *inst - Instruction definition
          char **operands - Array of operand strings
          int operand_start - Index of first operand
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord *instruction_word - Pointer to instruction word
Returns: int - TRUE if encoding succeeded, FALSE otherwise
*/
static int encode_two_operands(const Instruction *inst, char **operands, int operand_start, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
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
    if ((src_mode == ADDR_MODE_REGISTER) &&
        (dest_mode == ADDR_MODE_REGISTER))
        return store_two_registers(memory, current_ic_ptr, src_operand_word, dest_operand_word);
    else {
        /* Store source operand */
        if (!process_operand_storage(memory, current_ic_ptr, src_mode, src_operand_word, src_next_operand_word, REG_SRC_SHIFT))
            return FALSE;

        /* Store destination operand */
        return process_operand_storage(memory, current_ic_ptr, dest_mode, dest_operand_word, dest_next_operand_word, REG_DST_SHIFT);
    }
}

/* Outer methods */
/* ==================================================================== */
/*
Function to parse operands from tokenized line, determines operand start index and count.
Receives: char **tokens - Array of tokens from line
          int token_count - Total number of tokens
          int *operand_start_index - Output for operand start index
          int *operand_count - Output for number of operands
Returns: int - Always TRUE (validation done elsewhere)
*/
int parse_operands(char **tokens, int token_count, int *operand_start_index, int *operand_count) {
    int i;
    *operand_start_index = 0;

    if (is_first_token_label(tokens, token_count))
        *operand_start_index = 2; /* Skip label and instruction name */
    else
        *operand_start_index = 1; /* Skip only instruction name */

    *operand_count = 0;

    for (i = *operand_start_index; i < token_count; i++) {
        (*operand_count)++;
    }
    return TRUE;
}

/*
Function to validate operands against instruction requirements.
Checks addressing modes and operand count.
Receives: const Instruction *inst - Instruction definition
          char **operands - Array of operand strings
          int operand_count - Number of operands
Returns: int - TRUE if operands are valid, FALSE otherwise
*/
int check_operands(const Instruction *inst, char **operands, int operand_count) {
    int i, addr_mode_flag, legal_modes;

    for (i = 0; i < operand_count; i++) {
        addr_mode_flag = get_addressing_mode(operands[i]);

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

/*
Function to encode the instruction word (opcode and ARE bits), allocates space in memory image.
Receives: const Instruction *inst - Instruction definition
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord **current_word_ptr - Output for instruction word
Returns: int - TRUE if encoding succeeded, FALSE on IC limit
*/
int encode_instruction_word(const Instruction *inst, MemoryImage *memory, int *current_ic_ptr, MemoryWord **current_word_ptr) {
    if (!check_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    *current_word_ptr = &memory->words[IC_START + *current_ic_ptr];
    (*current_ic_ptr)++;

    clear_bits(*current_word_ptr);
    (*current_word_ptr)->instr.opcode = inst->opcode;
    (*current_word_ptr)->instr.are = ARE_ABSOLUTE;

    return TRUE;
}

/*
Function to route to appropriate encoder based on operand count.
Receives: const Instruction *inst - Instruction definition
          char **operands - Array of operand strings
          int operand_start - Index of first operand
          int operand_count - Number of operands
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current IC value
          MemoryWord *instruction_word - Pointer to instruction word
Returns: int - TRUE if encoding succeeded, FALSE otherwise
*/
int encode_operands(const Instruction *inst, char **operands, int operand_start, int operand_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    if (inst->num_operands == NO_OPERANDS) 
        return TRUE;

    if (inst->num_operands == ONE_OPERAND)
        return encode_one_operand(inst, operands, operand_start, symtab, memory, current_ic_ptr, instruction_word);

    else if (inst->num_operands == TWO_OPERANDS)
        return encode_two_operands(inst, operands, operand_start, symtab, memory, current_ic_ptr, instruction_word);

    return FALSE;
}