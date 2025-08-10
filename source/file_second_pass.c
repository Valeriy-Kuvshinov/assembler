#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"
#include "memory.h"
#include "instructions.h"
#include "directives.h"
#include "symbol_table.h"
#include "base4.h"
#include "line_process.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int encode_immediate_operand(const char *operand, MemoryWord *word) {
    char *endptr;
    long value = strtol(operand + 1, &endptr, BASE10_ENCODING); /* Skip immediate prefix */
    
    if (*endptr != NULL_TERMINATOR) {
        print_error("Invalid immediate value", operand);
        return FALSE;
    }
    word->operand.value = value & WORD_MASK;
    word->operand.are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_register_operand(const char *operand, MemoryWord *word) {
    int reg = operand[1] - '0';
    
    if (reg < 0 || reg > (MAX_REGISTER - '0')) {
        print_error("invalid register", operand);
        return FALSE;
    }
    word->operand.value = reg;
    word->operand.are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_symbol_operand(const char *operand, SymbolTable *symbol_table, MemoryWord *word) {
    Symbol *sym = find_symbol(symbol_table, operand);
    
    if (!sym) {
        print_error("Symbol not found", operand);
        return FALSE;
    }

    if (sym->type == EXTERNAL_SYMBOL) {
        word->operand.value = 0; /* External symbols have value 0 in the instruction word */
        word->operand.are = ARE_EXTERNAL;
        word->operand.ext_symbol_index = (int)(sym - symbol_table->symbols);
    } else {
        word->operand.value = sym->value & WORD_MASK;
        word->operand.are = ARE_RELOCATABLE;
        word->operand.ext_symbol_index = -1;
    }
    return TRUE;
}

static int extract_bracketed_registers(char *temp, char **reg1_str, char **reg2_str) {
    char *open1, *close1, *open2, *close2;

    open1 = strchr(temp, LEFT_BRACKET);

    if (!open1) {
        print_error(ERR_INVALID_MATRIX, "Missing first '['");
        return FALSE;
    }

    close1 = strchr(open1, RIGHT_BRACKET);

    if (!close1) {
        print_error(ERR_INVALID_MATRIX, "Missing first ']'");
        return FALSE;
    }

    open2 = strchr(close1, LEFT_BRACKET);

    if (!open2) {
        print_error(ERR_INVALID_MATRIX, "Missing second '['");
        return FALSE;
    }

    close2 = strchr(open2, RIGHT_BRACKET);

    if (!close2) {
        print_error(ERR_INVALID_MATRIX, "Missing second ']'");
        return FALSE;
    }
    *close1 = NULL_TERMINATOR;
    *close2 = NULL_TERMINATOR;
    *reg1_str = open1 + 1;
    *reg2_str = open2 + 1;

    return TRUE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *reg1_str, *reg2_str;

    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;

    if (!extract_bracketed_registers(temp, &reg1_str, &reg2_str))
        return FALSE;

    if (strlen(reg1_str) != REGISTER_LENGTH || reg1_str[0] != REGISTER_CHAR || !isdigit(reg1_str[1]) ||
        strlen(reg2_str) != REGISTER_LENGTH || reg2_str[0] != REGISTER_CHAR || !isdigit(reg2_str[1])) {
        print_error("Invalid register pair", "Invalid register format in matrix addressing");
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

static int encode_matrix_operand(const char *operand, SymbolTable *symbol_table, MemoryWord *word, MemoryWord *next_word) {
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
    if (!encode_symbol_operand(label, symbol_table, word))
        return FALSE;

    printf("DEBUG: matrix: label=%s base_reg=%d index_reg=%d\n", label, base_reg, index_reg);

    /* Encode the register part (second word of matrix operand) */
    next_word->raw = 0; /* Clear bits before setting */
    next_word->reg.reg_src = base_reg; /* Assign to reg_src */
    next_word->reg.reg_dst = index_reg; /* Assign to reg_dst */
    next_word->reg.are = ARE_ABSOLUTE; /* Register word is always absolute */
    /* ext_symbol_index is not part of RegWord, so do not access it here */

    printf("DEBUG: encoded matrix word = %d (0x%X)\n", next_word->raw, next_word->raw); /* Print raw value */

    return TRUE;
}

static int encode_operand(const char *operand, SymbolTable *symbol_table, MemoryWord *word, int is_dest, MemoryWord *next_word) {
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
        if (!encode_matrix_operand(operand, symbol_table, word, next_word)) 
            return FALSE;
    }
    /* Symbol/label (direct addressing) */
    else if (!encode_symbol_operand(operand, symbol_table, word)) 
        return FALSE;

    printf("DEBUG encode_operand: operand=\"%s\" word->raw=0x%03X\n", operand, word->raw & WORD_MASK);
    if (next_word)
        printf("DEBUG encode_operand: operand=\"%s\" next_word->raw=0x%03X\n", operand, next_word->raw & WORD_MASK);

    return TRUE;
}

static int store_operand_word(MemoryImage *memory, int *current_ic_ptr, MemoryWord operand_word) {
    int addr_index;

    if (!validate_ic_limit(*current_ic_ptr + 1))
        return FALSE;
    
    /* Store the word at the calculated instruction address */
    addr_index = INSTRUCTION_START + (*current_ic_ptr);
    memory->words[addr_index] = operand_word;
    (*current_ic_ptr)++;

    printf("DEBUG store_operand_word: stored at %d (base addr %d) value=0x%03X\n",
           addr_index, addr_index, memory->words[addr_index].raw & WORD_MASK);

    return TRUE;
}

static int store_register_word(MemoryImage *memory, int *current_ic_ptr, int reg_value, int shift) {
    MemoryWord *target_word;

    if (!validate_ic_limit(*current_ic_ptr + 1))
        return FALSE;
    
    target_word = &memory->words[INSTRUCTION_START + (*current_ic_ptr)];
    target_word->raw = 0;
    
    if (shift == 6) /* Source register */
        target_word->reg.reg_src = reg_value;
    else if (shift == 2)/* Destination register */
        target_word->reg.reg_dst = reg_value;
    
    target_word->reg.are = ARE_ABSOLUTE;
    (*current_ic_ptr)++;

    return TRUE;
}

static int store_two_registers(MemoryImage *memory, int *current_ic_ptr, MemoryWord src_word, MemoryWord dest_word) {
    MemoryWord *target_word;

    if (!validate_ic_limit(*current_ic_ptr + 1))
        return FALSE;
    
    target_word = &memory->words[INSTRUCTION_START + (*current_ic_ptr)];
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

static int handle_single_operand_encoding(const Instruction *inst, char **operands, int operand_start, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    int src_mode;
    MemoryWord operand_word, next_operand_word; 
    operand_word.raw = 0;
    next_operand_word.raw = 0;

    /* Encode the single operand (which is always the destination) */
    if (!encode_operand(operands[operand_start], symbol_table, &operand_word, FALSE, &next_operand_word))
        return FALSE;
    
    src_mode = get_operand_addressing_mode(operands[operand_start]);

    instruction_word->instr.dest = src_mode;
    instruction_word->instr.src = 0;

    printf("DEBUG handle_single_operand: instr at IC=%d instruction_word->raw=0x%03X\n",
           *current_ic_ptr - 1, instruction_word->raw & WORD_MASK);
    printf("DEBUG handle_single_operand: operand_word.raw=0x%03X next_word.raw=0x%03X\n",
           operand_word.raw & WORD_MASK, next_operand_word.raw & WORD_MASK);

    return process_operand_storage(memory, current_ic_ptr, src_mode, operand_word, next_operand_word, 2);
}

static int handle_two_operand_encoding(const Instruction *inst, char **operands, int operand_start, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    int src_mode, dest_mode;
    MemoryWord src_operand_word, src_next_operand_word, dest_operand_word, dest_next_operand_word; 
    src_operand_word.raw = 0;
    src_next_operand_word.raw = 0;
    dest_operand_word.raw = 0;
    dest_next_operand_word.raw = 0;

    /* Encode source operand */
    if (!encode_operand(operands[operand_start], symbol_table, &src_operand_word, FALSE, &src_next_operand_word))
        return FALSE;
    
    src_mode = get_operand_addressing_mode(operands[operand_start]);
    instruction_word->instr.src = src_mode; /* Set source addressing mode in instruction word */

    /* Encode destination operand */
    if (!encode_operand(operands[operand_start + 1], symbol_table, &dest_operand_word, TRUE, &dest_next_operand_word))
        return FALSE;
    
    dest_mode = get_operand_addressing_mode(operands[operand_start + 1]);
    instruction_word->instr.dest = dest_mode; /* Set destination addressing mode in instruction word */

    /* Two-register optimization: if both source and destination are registers, they share one word */
    if (src_mode == ADDR_MODE_REGISTER && dest_mode == ADDR_MODE_REGISTER)
        return store_two_registers(memory, current_ic_ptr, src_operand_word, dest_operand_word);
    else {
        /* Store source operand (if not part of two-register optimization) */
        if (!process_operand_storage(memory, current_ic_ptr, src_mode, src_operand_word, src_next_operand_word, 6))
            return FALSE;
        
        /* Store destination operand */
        return process_operand_storage(memory, current_ic_ptr, dest_mode, dest_operand_word, dest_next_operand_word, 2);
    }
}

static int encode_instruction_operands(const Instruction *inst, char **operands, int operand_start, int operand_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr, MemoryWord *instruction_word) {
    if (inst->num_operands == NO_OPERANDS) 
        return TRUE;
    
    if (inst->num_operands == ONE_OPERAND)
        return handle_single_operand_encoding(inst, operands, operand_start, symbol_table, memory, current_ic_ptr, instruction_word);
    else if (inst->num_operands == TWO_OPERANDS)
        return handle_two_operand_encoding(inst, operands, operand_start, symbol_table, memory, current_ic_ptr, instruction_word);

    return FALSE;
}

static int encode_instruction_word(const Instruction *inst, MemoryImage *memory, int *current_ic_ptr, MemoryWord **current_word_ptr) {
    if (!validate_ic_limit(*current_ic_ptr + 1))
        return FALSE;

    *current_word_ptr = &memory->words[INSTRUCTION_START + (*current_ic_ptr)++];
    
    (*current_word_ptr)->raw = 0; 
    
    /* Set the opcode and A/R/E bits using the InstrWord struct members. */
    (*current_word_ptr)->instr.opcode = inst->opcode;
    (*current_word_ptr)->instr.are = ARE_ABSOLUTE; /* Instruction word itself is always absolute */

    printf("DEBUG: Encoded instruction %s at IC=%d: opcode=%d (0x%x)\n", 
           inst->name, *current_ic_ptr - 1, inst->opcode, (*current_word_ptr)->raw);

    return TRUE;
}

static int parse_instruction_operands(char **tokens, int token_count, int *operand_start_idx, int *operand_count) {
    int i;
    *operand_start_idx = 0;
    
    /* Determine if there's a label. If so, operands start after label and instruction name. */
    if (has_label_in_tokens(tokens, token_count))
        *operand_start_idx = 2; /* Skip label and instruction name */
    else
        *operand_start_idx = 1; /* Skip only instruction name */
    
    *operand_count = 0;

    for (i = *operand_start_idx; i < token_count; i++) {
        (*operand_count)++;
    }
    return TRUE;
}

static int encode_instruction(const Instruction *inst, char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr) {
    int operand_start_idx, operand_count;
    MemoryWord *instruction_word;
    
    /* Parse operands to get their starting index and count */
    if (!parse_instruction_operands(tokens, token_count, &operand_start_idx, &operand_count))
        return FALSE;
    
    /* Validate addressing modes for the instruction's operands */
    if (!validate_instruction_operands_addressing_modes(inst, tokens + operand_start_idx, operand_count)) 
        return FALSE;
    
    /* Encode the first word of the instruction (opcode, ARE, addressing modes) */
    if (!encode_instruction_word(inst, memory, current_ic_ptr, &instruction_word))
        return FALSE;
    
    /* Encode the subsequent operand words */
    return encode_instruction_operands(inst, tokens, operand_start_idx, operand_count, symbol_table, memory, current_ic_ptr, instruction_word);
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr) {
    int instruction_name_idx;
    const Instruction *inst;
    
    /* Determine the index of the instruction name (skip label if present) */
    if (has_label_in_tokens(tokens, token_count))
        instruction_name_idx = 1;
    else
        instruction_name_idx = 0;
    
    inst = get_instruction(tokens[instruction_name_idx]);

    if (!inst) {
        print_error("Unknown instruction", tokens[instruction_name_idx]);
        return FALSE;
    }
    /* Encode the instruction and its operands into memory */
    return encode_instruction(inst, tokens, token_count, symbol_table, memory, current_ic_ptr);
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory) {
    int directive_idx;
    
    /* Determine the index of the directive (skip label if present) */
    if (has_label_in_tokens(tokens, token_count))
        directive_idx = 1;
    else
        directive_idx = 0;
    
    /* Check if there's actually a directive token after a potential label */
    if (directive_idx >= token_count) {
        print_error("Missing operand", "Missing directive after label");
        return FALSE;
    }
    return process_directive(tokens + directive_idx, token_count - directive_idx, symbol_table, memory, TRUE);
}

static int process_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr, int line_num) {
    if (!validate_line_format(tokens, token_count)) {
        fprintf(stderr, "Line %d: Invalid line format \n", line_num);
        return FALSE;
    }

    /* Check if it's a directive line */
    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symbol_table, memory)) {
            fprintf(stderr, "Line %d: Directive error \n", line_num);
            return FALSE;
        }
    } else { /* Must be an instruction line */
        if (!process_instruction_line(tokens, token_count, symbol_table, memory, current_ic_ptr)) {
            fprintf(stderr, "Line %d: Encoding error \n", line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symbol_table, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0;
    int second_pass_ic = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Parse the line into tokens */
        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error \n", line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        if (!process_line(tokens, token_count, symbol_table, memory, &second_pass_ic, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    return error_flag ? FALSE : TRUE;
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[5], dc_str[5];
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    /* Write header */
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    fprintf(fp, "%s %s\n", ic_str, dc_str);
    
    /* Write instructions */
    for (i = 0; i < memory->ic; i++) {
        int current_address = INSTRUCTION_START + i;
        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[current_address].raw, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    
    /* Write data */
    for (i = 0; i < memory->dc; i++) {
        int current_address = INSTRUCTION_START + memory->ic + i;
        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[i].raw, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    safe_fclose(&fp);
}

static void write_entry_file(const char *filename, SymbolTable *symbol_table) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (i = 0; i < symbol_table->count; i++) {
        if (symbol_table->symbols[i].is_entry) {
            convert_to_base4_address(symbol_table->symbols[i].value, addr_str);
            fprintf(fp, "%s %s\n", symbol_table->symbols[i].name, addr_str);
        }
    }
    safe_fclose(&fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symbol_table) {
    char addr_str[ADDR_LENGTH];
    int j;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (j = INSTRUCTION_START; j < INSTRUCTION_START + memory->ic; j++) {
        /* Check if the word at this instruction address is an external reference */
        if (memory->words[j].operand.are == ARE_EXTERNAL && memory->words[j].operand.ext_symbol_index >= 0) {
            convert_to_base4_address(j, addr_str);
            fprintf(fp, "%s %s\n", symbol_table->symbols[memory->words[j].operand.ext_symbol_index].name, addr_str);
        }
    }
    safe_fclose(&fp);
}

static void write_output_files(MemoryImage *memory, SymbolTable *symbol_table, const char *obj_file, const char *ent_file, const char *ext_file) {
    int i;
    int external_ref_count = 0;
    
    write_object_file(obj_file, memory);

    if (has_entries(symbol_table))
        write_entry_file(ent_file, symbol_table);

    if (has_externs(symbol_table)) {
        /* Count external references */
        for (i = INSTRUCTION_START; i < INSTRUCTION_START + memory->ic; i++) {
            if (memory->words[i].operand.are == ARE_EXTERNAL)
                external_ref_count++;
        }        
        if (external_ref_count > 0)
            write_extern_file(ext_file, memory, symbol_table);
    }
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symbol_table, MemoryImage *memory, const char *obj_file, const char *ent_file, const char *ext_file) {
    FILE *fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(fp, symbol_table, memory)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }
    safe_fclose(&fp);
    write_output_files(memory, symbol_table, obj_file, ent_file, ext_file);

    return TRUE;
}