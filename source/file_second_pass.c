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
#include "base4.h"
#include "line_process.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int get_addressing_mode(const char *operand) {
    if (operand[0] == IMMEDIATE_PREFIX)
        return ADDR_MODE_IMMEDIATE;
    else if (strchr(operand, LEFT_BRACKET))
        return ADDR_MODE_MATRIX;
    else if (operand[0] == REGISTER_CHAR && isdigit(operand[1]) && operand[2] == NULL_TERMINATOR)
        return ADDR_MODE_REGISTER;
    else
        return ADDR_MODE_DIRECT;
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
    int reg = operand[1] - MIN_REGISTER;
    
    if (reg < 0 || reg > 7) {
        print_error("Invalid register (must be r0-r7)", operand);
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
                word->are = (symtab->symbols[i].type == DATA_SYMBOL) ? ARE_RELOCATABLE : ARE_ABSOLUTE;
            }
            return TRUE;
        }
    }
    print_error("Label not defined", operand);

    return FALSE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *open1, *close1, *open2, *close2, *reg1_str, *reg2_str;
    
    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
    
    open1 = strchr(temp, LEFT_BRACKET);

    if (!open1) 
        return FALSE;
    
    close1 = strchr(open1, RIGHT_BRACKET);

    if (!close1) 
        return FALSE;
    
    open2 = strchr(close1, LEFT_BRACKET);

    if (!open2) 
        return FALSE;
    
    close2 = strchr(open2, RIGHT_BRACKET);

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
    
    return (*base_reg >= 0 && *base_reg <= 7 &&
            *index_reg >= 0 && *index_reg <= 7);
}

static int encode_matrix_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, MemoryWord *next_word) {
    char label[MAX_LABEL_NAME_LENGTH];
    char *bracket = strchr(operand, LEFT_BRACKET);
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
    word->ext_symbol_index = -1;

    if (next_word) 
        next_word->ext_symbol_index = -1;
    
    /* Immediate value (#num) */
    if (operand[0] == IMMEDIATE_PREFIX)
        return encode_immediate_operand(operand, word);
    
    /* Register (r0-r7) */
    if (operand[0] == REGISTER_CHAR && isdigit(operand[1]) && operand[2] == NULL_TERMINATOR)
        return encode_register_operand(operand, word);
    
    /* Matrix access (label[rX][rY]) */
    if (strchr(operand, LEFT_BRACKET))
        return encode_matrix_operand(operand, symtab, word, next_word);
    
    return encode_symbol_operand(operand, symtab, word);
}

static int validate_addressing_modes(const Instruction *inst, char **operands, int operand_count) {
    int i;
    
    for (i = 0; i < operand_count; i++) {
        int addr_mode, legal_modes;
        
        addr_mode = (operands[i][0] == IMMEDIATE_PREFIX) ? ADDR_FLAG_IMMEDIATE :
                   (strchr(operands[i], LEFT_BRACKET)) ? ADDR_FLAG_MATRIX :
                   (operands[i][0] == REGISTER_CHAR && isdigit(operands[i][1]) 
                    && operands[i][2] == NULL_TERMINATOR) ? ADDR_FLAG_REGISTER :
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

static int encode_instruction_operands(const Instruction *inst, char **operands, int operand_start, int operand_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic, MemoryWord *current_word) {
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
        
        /* Store operands */
        if (src_mode == ADDR_MODE_REGISTER && dest_mode == ADDR_MODE_REGISTER) {
            /* Two-register optimization */
            memory->words[INSTRUCTION_START + *current_ic].value = 
                (operand_word.value << 6) | (dest_operand_word.value << 2);
            memory->words[INSTRUCTION_START + *current_ic].are = ARE_ABSOLUTE;
            memory->words[INSTRUCTION_START + *current_ic].ext_symbol_index = -1;
            (*current_ic)++;
        } else {
            /* Store source operand */
            if (src_mode == ADDR_MODE_REGISTER) {
                /* Store register in high bits */
                memory->words[INSTRUCTION_START + *current_ic].value = operand_word.value << 6;
                memory->words[INSTRUCTION_START + *current_ic].are = ARE_ABSOLUTE;
                memory->words[INSTRUCTION_START + *current_ic].ext_symbol_index = -1;
                (*current_ic)++;
            } else {
                memory->words[INSTRUCTION_START + *current_ic] = operand_word;
                (*current_ic)++;
                if (src_mode == ADDR_MODE_MATRIX) {
                    memory->words[INSTRUCTION_START + *current_ic] = next_operand_word;
                    (*current_ic)++;
                }
            }
            
            /* Store destination operand */
            if (dest_mode == ADDR_MODE_REGISTER) {
                /* Store register in low bits */
                memory->words[INSTRUCTION_START + *current_ic].value = operand_word.value << 2;
                memory->words[INSTRUCTION_START + *current_ic].are = ARE_ABSOLUTE;
                memory->words[INSTRUCTION_START + *current_ic].ext_symbol_index = -1;
                (*current_ic)++;
            } else {
                memory->words[INSTRUCTION_START + *current_ic] = dest_operand_word;
                (*current_ic)++;
                if (dest_mode == ADDR_MODE_MATRIX) {
                    memory->words[INSTRUCTION_START + *current_ic] = dest_next_operand_word;
                    (*current_ic)++;
                }
            }
        }
    } else {
        /* Single operand instruction */
        if (src_mode == ADDR_MODE_REGISTER) {
            memory->words[INSTRUCTION_START + *current_ic].value = operand_word.value << 2;
            memory->words[INSTRUCTION_START + *current_ic].are = ARE_ABSOLUTE;
            memory->words[INSTRUCTION_START + *current_ic].ext_symbol_index = -1;
            (*current_ic)++;
        } else {
            memory->words[INSTRUCTION_START + *current_ic] = operand_word;
            (*current_ic)++;
            if (src_mode == ADDR_MODE_MATRIX) {
                memory->words[INSTRUCTION_START + *current_ic] = next_operand_word;
                (*current_ic)++;
            }
        }
    }
    return TRUE;
}

static int encode_instruction_word(const Instruction *inst, MemoryImage *memory, int *current_ic, MemoryWord **current_word) {
    int opcode;
    
    *current_word = &memory->words[INSTRUCTION_START + (*current_ic)++];
    
    printf("DEBUG: Before encoding %s: current_ic = %d \n", inst->name, *current_ic);
    
    opcode = get_instruction_opcode(inst->name);

    if (opcode == -1) {
        print_error(ERR_UNKNOWN_INSTRUCTION, inst->name);
        return FALSE;
    }

    (*current_word)->value = opcode << OPCODE_SHIFT;
    (*current_word)->are = ARE_ABSOLUTE;
    (*current_word)->ext_symbol_index = -1;

    printf("DEBUG: After encoding %s: current_ic = %d \n", inst->name, *current_ic);
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
    
    if (!parse_instruction_operands(operands, &operand_start, &operand_count))
        return FALSE;
    
    if (!validate_addressing_modes(inst, operands + operand_start, operand_count)) 
        return FALSE;
    
    if (!encode_instruction_word(inst, memory, current_ic, &current_word))
        return FALSE;
    
    /* Encode operands */
    return encode_instruction_operands(inst, operands, operand_start, operand_count, symtab, memory, current_ic, current_word);
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic) {
    int instruction_index, operands_start;
    const Instruction *inst;
    int has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label) {
        instruction_index = 1;
        operands_start = 2;
    } else {
        instruction_index = 0;
        operands_start = 1;
    }
    
    inst = get_instruction(tokens[instruction_index]);

    if (!inst) {
        print_error(ERR_UNKNOWN_INSTRUCTION, tokens[instruction_index]);
        return FALSE;
    }
    return encode_instruction(inst, tokens + operands_start, symtab, memory, current_ic);
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int directive_index;
    int has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label)
        directive_index = 1;
    else
        directive_index = 0;
    
    /* Check bounds */
    if (directive_index >= token_count) {
        print_error("Missing directive after label", NULL);
        return FALSE;
    }
    return process_directive(tokens + directive_index, token_count - directive_index, symtab, memory, TRUE);
}

static int process_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic, int line_num) {
    if (!validate_line_format(tokens, token_count)) {
        fprintf(stderr, "Line %d: Invalid line format \n", line_num);
        return FALSE;
    }

    if (*current_ic >= MAX_IC_SIZE) {
        fprintf(stderr, "Memory overflow at IC=%d \n", *current_ic);
        return FALSE;
    }

    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symtab, memory)) {
            fprintf(stderr, "Line %d: Directive error \n", line_num);
            return FALSE;
        }
    } else { /* Must be instruction line */
        if (!process_instruction_line(tokens, token_count, symtab, memory, current_ic)) {
            fprintf(stderr, "Line %d: Encoding error \n", line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[10], dc_str[10];
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    /* Write header */
    printf("DEBUG: Writing header - IC = %d, DC = %d \n", memory->ic, memory->dc);
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    printf("DEBUG: Converted to base4 - IC = %s, DC = %s \n", ic_str, dc_str);
    
    fprintf(fp, "%s %s\n", ic_str, dc_str);
    
    /* Write instructions */
    for (i = INSTRUCTION_START; i < INSTRUCTION_START + memory->ic; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    
    /* Write data */
    for (i = INSTRUCTION_START + memory->ic; i < INSTRUCTION_START + memory->ic + memory->dc; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    safe_fclose(&fp);
}

static void write_entry_file(const char *filename, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) {
            convert_to_base4_address(symtab->symbols[i].value, addr_str);
            fprintf(fp, "%s %s\n", symtab->symbols[i].name, addr_str);
        }
    }
    safe_fclose(&fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int j;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (j = INSTRUCTION_START; j < INSTRUCTION_START + memory->ic; j++) {
        if (memory->words[j].are == ARE_EXTERNAL && memory->words[j].ext_symbol_index >= 0) {
            convert_to_base4_address(j, addr_str);
            fprintf(fp, "%s %s\n", 
                    symtab->symbols[memory->words[j].ext_symbol_index].name, 
                    addr_str);
        }
    }
    safe_fclose(&fp);
}

static void write_output_files(MemoryImage *memory, SymbolTable *symtab, const char *obj_file, const char *ent_file, const char *ext_file) {
    int i, external_ref_count = 0;
    
    write_object_file(obj_file, memory);

    if (has_entries(symtab))
        write_entry_file(ent_file, symtab);

    if (has_externs(symtab)) {
        /* Count external references */
        for (i = INSTRUCTION_START; i < INSTRUCTION_START + memory->ic; i++) {
            if (memory->words[i].are == ARE_EXTERNAL) {
                printf("DEBUG: Found external reference at address %d \n", i);
                external_ref_count++;
            }
        }
        
        if (external_ref_count > 0)
            write_extern_file(ext_file, memory, symtab);
    }
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory, const char *obj_file, const char *ent_file, const char *ext_file) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0;
    int line_num = 0, error_flag = 0, current_ic = 0;
    FILE *fp = open_source_file(filename);
    
    if (!fp)
        return PASS_ERROR;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error \n", line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0)
            continue;

        if (!process_line(tokens, token_count, symtab, memory, &current_ic, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    safe_fclose(&fp);

    if (!error_flag)
        write_output_files(memory, symtab, obj_file, ent_file, ext_file);
    
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}