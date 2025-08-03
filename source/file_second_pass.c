#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"
#include "instructions.h"
#include "directives.h"
#include "symbol_table.h"
#include "file_io.h"

/* Inner STATIC methods - Utility Functions */
/* ==================================================================== */
static int has_label_in_tokens(char **tokens, int token_count) {
    int i;

    for (i = 0; i < token_count; i++) {
        if (strchr(tokens[i], LABEL_TERMINATOR))
            return TRUE;
    }
    return FALSE;
}

static int is_directive_line(char **tokens, int has_label) {
    return (tokens[has_label ? 1 : 0][0] == DIRECTIVE_CHAR);
}

static int get_addressing_mode(const char *operand) {
    if (operand[0] == IMMEDIATE_PREFIX)
        return ADDR_MODE_IMMEDIATE;
    else if (strchr(operand, MATRIX_LEFT_BRACKET))
        return ADDR_MODE_MATRIX;
    else if (operand[0] == REGISTER_CHAR && isdigit(operand[1]) && operand[2] == NULL_TERMINATOR)
        return ADDR_MODE_REGISTER;
    else
        return ADDR_MODE_DIRECT;
}

static int check_memory_overflow(int current_ic) {
    if (current_ic >= WORD_COUNT) {
        print_error("Code section overflow", NULL);
        return FALSE;
    }
    return TRUE;
}

static int encode_immediate_operand(const char *operand, MemoryWord *word) {
    char *endptr;
    long value = strtol(operand + 1, &endptr, 10);
    
    if (*endptr != NULL_TERMINATOR) {
        print_error("Invalid immediate value", operand);
        return FALSE;
    }
    
    word->value = value & WORD_MASK;
    word->are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_register_operand(const char *operand, MemoryWord *word) {
    int reg = operand[1] - '0';
    
    if (reg < 0 || reg > MAX_REGISTER) {
        print_error(ERR_INVALID_REGISTER, operand);
        return FALSE;
    }
    
    word->value = reg;
    word->are = ARE_ABSOLUTE;

    return TRUE;
}

static int encode_symbol_operand(const char *operand, SymbolTable *symtab, MemoryWord *word) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, operand) == 0) {
            if (symtab->symbols[i].type == EXTERNAL_SYMBOL) {
                word->value = 0;
                word->are = ARE_EXTERNAL;
                word->ext_symbol_index = i;
            } else {
                word->value = symtab->symbols[i].value;
                word->are = (symtab->symbols[i].type == DATA_SYMBOL) ? 
                           ARE_RELOCATABLE : ARE_ABSOLUTE;
            }
            return TRUE;
        }
    }
    print_error(ERR_LABEL_NOT_DEFINED, operand);

    return FALSE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *open1, *close1, *open2, *close2, *reg1_str, *reg2_str;
    
    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
    
    open1 = strchr(temp, MATRIX_LEFT_BRACKET); /* Find brackets */

    if (!open1) 
        return FALSE;
    
    close1 = strchr(open1, MATRIX_RIGHT_BRACKET);

    if (!close1) 
        return FALSE;
    
    open2 = strchr(close1, MATRIX_LEFT_BRACKET);

    if (!open2) 
        return FALSE;
    
    close2 = strchr(open2, MATRIX_RIGHT_BRACKET);

    if (!close2) 
        return FALSE;
    
    /* Extract register numbers */
    *close1 = NULL_TERMINATOR;
    *close2 = NULL_TERMINATOR;
    reg1_str = open1 + 1;
    reg2_str = open2 + 1;
    
    /* Validate register format */
    if (reg1_str[0] != REGISTER_CHAR || reg2_str[0] != REGISTER_CHAR) 
        return FALSE;
    
    *base_reg = atoi(reg1_str + 1);
    *index_reg = atoi(reg2_str + 1);
    
    return (*base_reg >= 0 && *base_reg <= MAX_REGISTER && *index_reg >= 0 && *index_reg <= MAX_REGISTER);
}

static int encode_matrix_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, MemoryWord *next_word) {
    char label[MAX_LABEL_LENGTH];
    char *bracket = strchr(operand, MATRIX_LEFT_BRACKET);
    int base_reg, index_reg;

    if (!next_word) {
        print_error("Matrix addressing requires next word", operand);
        return FALSE;
    }

    /* Extract label part */
    strncpy(label, operand, bracket - operand);
    label[bracket - operand] = NULL_TERMINATOR;

    printf("DEBUG: Extracted matrix label '%s' from operand '%s'\n", label, operand);

    /* Parse matrix registers */
    if (!parse_matrix_operand(operand, &base_reg, &index_reg)) {
        print_error("Invalid matrix addressing", operand);
        return FALSE;
    }

    /* Encode the label part */
    if (!encode_symbol_operand(label, symtab, word))
        return FALSE;

    /* Store registers in next word */
    next_word->value = (base_reg << 6) | (index_reg << 2);
    next_word->are = ARE_ABSOLUTE;
    next_word->ext_symbol_index = -1;

    return TRUE;
}

static int encode_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, int is_dest, MemoryWord *next_word) {
    word->ext_symbol_index = -1; /* Initialize ext_symbol_index */

    if (next_word) 
        next_word->ext_symbol_index = -1;
    
    /* Immediate value (#num) */
    if (operand[0] == IMMEDIATE_PREFIX)
        return encode_immediate_operand(operand, word);
    
    /* Register (r0-r7) */
    if (operand[0] == REGISTER_CHAR && isdigit(operand[1]) && operand[2] == NULL_TERMINATOR)
        return encode_register_operand(operand, word);
    
    /* Matrix access (label[rX][rY]) */
    if (strchr(operand, MATRIX_LEFT_BRACKET))
        return encode_matrix_operand(operand, symtab, word, next_word);
    
    /* Symbol/label */
    return encode_symbol_operand(operand, symtab, word);
}

static int validate_addressing_modes(const Instruction *inst, char **operands, int operand_count) {
    int i;
    
    for (i = 0; i < operand_count; i++) {
        int addr_mode, legal_modes;
        
        addr_mode = (operands[i][0] == IMMEDIATE_PREFIX) ? ADDR_FLAG_IMMEDIATE :
                   (strchr(operands[i], MATRIX_LEFT_BRACKET)) ? ADDR_FLAG_MATRIX :
                   (operands[i][0] == REGISTER_CHAR && isdigit(operands[i][1]) && operands[i][2] == NULL_TERMINATOR) ? ADDR_FLAG_REGISTER :
                   ADDR_FLAG_DIRECT;
        
        /* Check if addressing mode is legal */
        switch (inst->num_operands) {
            case TWO_OPERANDS:
                legal_modes = (i == 0) ? inst->legal_src_addr_modes : inst->legal_dest_addr_modes;
                break;
            case ONE_OPERAND:
                legal_modes = inst->legal_dest_addr_modes;
                break;
            case NO_OPERANDS:
                legal_modes = 0;
                break;
            default:
                print_error(ERR_WRONG_OPERAND_COUNT, NULL);
                return FALSE;
        }
        
        if (!(legal_modes & addr_mode)) {
            print_error(ERR_INVALID_ADDRESSING, operands[i]);
            return FALSE;
        }
    }
    return TRUE;
}

static int store_operand_word(MemoryImage *memory, int *current_ic, MemoryWord operand_word) {
    if (!check_memory_overflow(*current_ic))
        return FALSE;
    
    memory->words[INSTRUCTION_START + (*current_ic)++] = operand_word;

    return TRUE;
}

static int store_register_word(MemoryImage *memory, int *current_ic, int reg_value, int shift) {
    if (!check_memory_overflow(*current_ic))
        return FALSE;
    
    memory->words[INSTRUCTION_START + (*current_ic)++].value = reg_value << shift;
    memory->words[INSTRUCTION_START + *current_ic - 1].are = ARE_ABSOLUTE;
    memory->words[INSTRUCTION_START + *current_ic - 1].ext_symbol_index = -1;

    return TRUE;
}

static int store_two_registers(MemoryImage *memory, int *current_ic, MemoryWord src_word, MemoryWord dest_word) {
    if (!check_memory_overflow(*current_ic))
        return FALSE;
    
    memory->words[INSTRUCTION_START + (*current_ic)++].value = (src_word.value << 6) | (dest_word.value << 2);
    memory->words[INSTRUCTION_START + *current_ic - 1].are = ARE_ABSOLUTE;
    memory->words[INSTRUCTION_START + *current_ic - 1].ext_symbol_index = -1;

    return TRUE;
}

static int process_operand_storage(MemoryImage *memory, int *current_ic, int mode, MemoryWord operand_word, MemoryWord next_word, int reg_shift) {
    if (mode == ADDR_MODE_REGISTER)
        return store_register_word(memory, current_ic, operand_word.value, reg_shift);
    else {
        if (!store_operand_word(memory, current_ic, operand_word))
            return FALSE;
        
        if (mode == ADDR_MODE_MATRIX)
            return store_operand_word(memory, current_ic, next_word);
    }
    return TRUE;
}

static int encode_instruction_operands(const Instruction *inst, char **operands, int operand_start, int operand_count, 
                                     SymbolTable *symtab, MemoryImage *memory, int *current_ic, MemoryWord *current_word) {
    MemoryWord operand_word, next_operand_word, dest_operand_word, dest_next_operand_word;
    int src_mode, dest_mode;
    
    if (inst->num_operands < 1) 
        return TRUE;
    
    /* Encode source operand */
    if (!encode_operand(operands[operand_start], symtab, &operand_word, FALSE, &next_operand_word))
        return FALSE;
    
    src_mode = get_addressing_mode(operands[operand_start]);
    current_word->value |= (src_mode << SRC_SHIFT);
    
    /* Handle two operand instructions */
    if (inst->num_operands >= 2) {
        if (!encode_operand(operands[operand_start + 1], symtab, &dest_operand_word, TRUE, &dest_next_operand_word))
            return FALSE;
        
        dest_mode = get_addressing_mode(operands[operand_start + 1]);
        current_word->value |= (dest_mode << DEST_SHIFT);
        
        /* Two-register optimization */
        if (src_mode == ADDR_MODE_REGISTER && dest_mode == ADDR_MODE_REGISTER)
            return store_two_registers(memory, current_ic, operand_word, dest_operand_word);
        else {
            /* Store source operand */
            if (!process_operand_storage(memory, current_ic, src_mode, operand_word, next_operand_word, 6))
                return FALSE;
            
            /* Store destination operand */
            return process_operand_storage(memory, current_ic, dest_mode, dest_operand_word, dest_next_operand_word, 2);
        }
    } else
        /* Single operand instruction */
        return process_operand_storage(memory, current_ic, src_mode, operand_word, next_operand_word, 2);
}

static int encode_instruction_word(const Instruction *inst, MemoryImage *memory, int *current_ic, MemoryWord **current_word) {
    int opcode;
    
    if (!check_memory_overflow(*current_ic))
        return FALSE;
    
    *current_word = &memory->words[INSTRUCTION_START + (*current_ic)++];
    
    printf("DEBUG: Before encoding %s: current_ic = %d\n", inst->name, *current_ic);
    
    opcode = get_instruction_opcode(inst->name);

    if (opcode == -1) {
        print_error(ERR_UNKNOWN_INSTRUCTION, inst->name);
        return FALSE;
    }

    (*current_word)->value = opcode << OPCODE_SHIFT;
    (*current_word)->are = ARE_ABSOLUTE;
    (*current_word)->ext_symbol_index = -1;

    printf("DEBUG: After encoding %s: current_ic = %d\n", inst->name, *current_ic);
    return TRUE;
}

static int parse_instruction_operands(char **operands, int *operand_start, int *operand_count) {
    int i;
    
    *operand_start = 0;
    
    /* Check for label (should have been processed in first pass) */
    for (i = 0; operands[i]; i++) {
        if (strchr(operands[i], LABEL_TERMINATOR)) {
            *operand_start = 1;  /* Skip the label when processing operands */
            break;
        }
    }
    
    /* Count actual operands (excluding label) */
    for (i = *operand_start, *operand_count = 0; operands[i]; i++) {
        (*operand_count)++;
    }
    return TRUE;
}

static int encode_instruction(const Instruction *inst, char **operands, SymbolTable *symtab, MemoryImage *memory, int *current_ic) {
    int operand_start, operand_count;
    MemoryWord *current_word;
    
    /* Parse operands */
    if (!parse_instruction_operands(operands, &operand_start, &operand_count))
        return FALSE;
    
    /* Validate addressing modes */
    if (!validate_addressing_modes(inst, operands + operand_start, operand_count)) 
        return FALSE;
    
    /* Encode instruction word */
    if (!encode_instruction_word(inst, memory, current_ic, &current_word))
        return FALSE;
    
    /* Encode operands */
    return encode_instruction_operands(inst, operands, operand_start, operand_count, symtab, memory, current_ic, current_word);
}

static int process_instruction_line(char **tokens, int has_label, SymbolTable *symtab, MemoryImage *memory, int *current_ic) {
    const Instruction *inst = get_instruction(tokens[has_label ? 1 : 0]);

    if (!inst) {
        print_error(ERR_UNKNOWN_INSTRUCTION, tokens[has_label ? 1 : 0]);
        return FALSE;
    }
    return encode_instruction(inst, tokens + (has_label ? 2 : 1), symtab, memory, current_ic);
}

static int process_single_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic, int line_num) {
    int has_label = has_label_in_tokens(tokens, token_count);
    int directive_idx = has_label ? 1 : 0;
    
    if (is_directive_line(tokens, has_label)) {
        /* Check bounds and process directive */
        if (directive_idx >= token_count) {
            fprintf(stderr, "Line %d: Missing directive\n", line_num);
            return FALSE;
        }
        
        if (!process_directive(tokens + directive_idx, token_count - directive_idx, symtab, memory, TRUE)) {
            fprintf(stderr, "Line %d: Directive error\n", line_num);
            return FALSE;
        }
    } else {
        /* Process instruction */
        if (!process_instruction_line(tokens, has_label, symtab, memory, current_ic)) {
            fprintf(stderr, "Line %d: Encoding error\n", line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int parse_and_process_line(const char *line, SymbolTable *symtab, MemoryImage *memory, int *current_ic, int line_num) {
    char **tokens = NULL;
    int token_count = 0;
    int success = TRUE;
    
    if (!parse_tokens(line, &tokens, &token_count)) {
        fprintf(stderr, "Line %d: Syntax error\n", line_num);
        return FALSE;
    }

    if (token_count > 0)
        success = process_single_line(tokens, token_count, symtab, memory, current_ic, line_num);
    
    free_tokens(tokens, token_count);
    return success;
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory,
                const char *obj_file, const char *ent_file, const char *ext_file) {
    char line[MAX_LINE_LENGTH];
    int line_num = 0, error_flag = 0, current_ic = 0;
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error(ERR_FILE_OPEN, filename);
        return PASS_ERROR;
    }

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        trim_whitespace(line);
        
        if (should_skip_line(line))
            continue;

        if (!parse_and_process_line(line, symtab, memory, &current_ic, line_num)) 
            error_flag = 1;
    }
    fclose(fp);

    /* Write output files if no errors */
    if (!error_flag)
        write_output_files(memory, symtab, obj_file, ent_file, ext_file);
    
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}