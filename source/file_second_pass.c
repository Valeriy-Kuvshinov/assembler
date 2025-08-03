#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"
#include "instructions.h"
#include "directives.h"
#include "base4.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int has_external_references(const MemoryImage *memory) {
    int i;
    int count = 0;
    
    for (i = IC_START; i < IC_START + memory->ic; i++) {
        if (memory->words[i].are == ARE_EXTERNAL) {
            printf("DEBUG: Found external reference at address %d\n", i);
            count++;
        }
    }
    printf("DEBUG: Total external references found: %d\n", count);
    return count > 0;
}

static int process_directive(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    if (token_count < 1) 
        return FALSE;
    
    if (strcmp(tokens[0], ENTRY_DIRECTIVE) == 0) 
        return process_entry_directive(tokens, token_count, symtab);

    return TRUE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *open1, *close1, *open2, *close2, *reg1_str, *reg2_str;
    
    /* Copy operand to temp buffer for manipulation */
    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
    
    /* Find brackets */
    open1 = strchr(temp, LEFT_BRACKET_CHAR);

    if (!open1) 
        return FALSE;

    close1 = strchr(open1, RIGHT_BRACKET_CHAR);

    if (!close1) 
        return FALSE;
    
    open2 = strchr(close1, LEFT_BRACKET_CHAR);
    if (!open2) 
        return FALSE;

    close2 = strchr(open2, RIGHT_BRACKET_CHAR);

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
    
    if (*base_reg < 0 || *base_reg > MAX_REGISTER ||
        *index_reg < 0 || *index_reg > MAX_REGISTER)
        return FALSE;
    
    return TRUE;
}

static int encode_operand(const char *operand, SymbolTable *symtab, MemoryWord *word, int is_dest, MemoryWord *next_word) {
    char *endptr;
    int i;
    long value;
    
    /* Initialize ext_symbol_index */
    word->ext_symbol_index = -1;
    if (next_word) next_word->ext_symbol_index = -1;
    
    /* Immediate value (#num) */
    if (operand[0] == IMMEDIATE_PREFIX) {
        value = strtol(operand + 1, &endptr, 10);
        if (*endptr != NULL_TERMINATOR) {
            print_error("Invalid immediate value", operand);
            return FALSE;
        }
        word->value = value & WORD_MASK;
        word->are = ARE_ABSOLUTE;
        return TRUE;
    }
    
    /* Register (r0-r7) */
    if (operand[0] == REGISTER_CHAR && isdigit(operand[1]) && operand[2] == NULL_TERMINATOR) {
        int reg = operand[1] - '0';

        if (reg < 0 || reg > MAX_REGISTER) {
            print_error("Invalid register", operand);
            return FALSE;
        }
        word->value = reg;
        word->are = ARE_ABSOLUTE;

        return TRUE;
    }
    
    /* Matrix access (label[rX][rY]) */
    if (strchr(operand, LEFT_BRACKET_CHAR)) {
        char label[MAX_LABEL_LENGTH];
        char *bracket = strchr(operand, LEFT_BRACKET_CHAR);
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
    
        /* Find label in symbol table */
        printf("DEBUG: Looking for symbol '%s' in symbol table\n", label);  /* Use label, not operand! */
        
        for (i = 0; i < symtab->count; i++) {
            printf("DEBUG: Symbol %d: '%s' (type=%d, value=%d)\n", i, symtab->symbols[i].name, symtab->symbols[i].type, symtab->symbols[i].value);
        
            if (strcmp(symtab->symbols[i].name, label) == 0) {  /* Compare with label, not operand! */
                printf("DEBUG: Found match for '%s'!\n", label);
            
                if (symtab->symbols[i].type == EXTERNAL_SYMBOL) {
                    printf("DEBUG: Encoding external symbol %s\n", operand);
                    word->value = 0;
                    word->are = ARE_EXTERNAL;
                    word->ext_symbol_index = i;
                } else {
                    word->value = symtab->symbols[i].value;
                    word->are = (symtab->symbols[i].type == DATA_SYMBOL) ? ARE_RELOCATABLE : ARE_ABSOLUTE;
                }
            
                /* Store registers in next word */
                next_word->value = (base_reg << 6) | (index_reg << 2);
                next_word->are = ARE_ABSOLUTE;
                next_word->ext_symbol_index = -1;

                return TRUE;
            }
        }
        printf("DEBUG: Symbol '%s' not found in table\n", label);  /* Use label, not operand! */
        print_error(ERR_UNKNOWN_LABEL, label);  /* Use label, not operand! */

        return FALSE;
    }
    
    /* Symbol/label */
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
    print_error(ERR_UNKNOWN_LABEL, operand);
    return FALSE;
}

static int validate_addressing_modes(const Instruction *inst, char **operands, int operand_count) {
    int i;
    
    for (i = 0; i < operand_count; i++) {
        int addr_mode, legal_modes;
        
        /* Determine addressing mode */
        if (operands[i][0] == IMMEDIATE_PREFIX)
            addr_mode = ADDR_FLAG_IMMEDIATE;
        else if (strchr(operands[i], LEFT_BRACKET_CHAR))
            addr_mode = ADDR_FLAG_MATRIX;
        else if (operands[i][0] == REGISTER_CHAR && isdigit(operands[i][1]) && operands[i][2] == NULL_TERMINATOR)
            addr_mode = ADDR_FLAG_REGISTER;
        else
            addr_mode = ADDR_FLAG_DIRECT;
        
        /* Check if addressing mode is legal based on operand count */
        switch (inst->num_operands) {
            case TWO_OPERANDS:
                legal_modes = (i == 0) ? inst->legal_src_addr_modes : inst->legal_dest_addr_modes;
                break;
            case ONE_OPERAND:
                legal_modes = inst->legal_dest_addr_modes; /* Single operand uses dest modes */
                break;
            case NO_OPERANDS:
                legal_modes = 0; /* No operands allowed */
                break;
            default:
                print_error("Invalid instruction operand count", NULL);
                return FALSE;
        }
        
        if (!(legal_modes & addr_mode)) {
            print_error(ERR_ILLEGAL_ADDRESSING_MODE, operands[i]);
            return FALSE;
        }
    }
    return TRUE;
}

static int encode_instruction(const Instruction *inst, char **operands, SymbolTable *symtab, MemoryImage *memory, int *current_ic) {
    int opcode, i;
    int operand_start = 0, operand_count = 0, src_mode = 0, dest_mode = 0;
    MemoryWord *current_word;
    
    /* Check for label (should have been processed in first pass) */
    for (i = 0; operands[i]; i++) {
        if (strchr(operands[i], LABEL_TERMINATOR)) {
            operand_start = 1;  /* Skip the label when processing operands */
            break;
        }
    }
    
    /* Count actual operands (excluding label) */
    for (i = operand_start; operands[i]; i++) {
        operand_count++;
    }
    
    /* Validate addressing modes */
    if (!validate_addressing_modes(inst, operands + operand_start, operand_count)) 
        return FALSE;
    
    /* Get current memory word using separate counter */
    if (*current_ic >= WORD_COUNT) {
        print_error("Code section overflow", NULL);
        return FALSE;
    }
    current_word = &memory->words[(*current_ic)++];
    
    printf("DEBUG: Before encoding %s: current_ic = %d\n", inst->name, *current_ic);
    opcode = get_instruction_opcode(inst->name);

    if (opcode == -1) {
        print_error("Invalid instruction", inst->name);
        return FALSE;
    }

    current_word->value = opcode << OPCODE_SHIFT;
    current_word->are = ARE_ABSOLUTE;
    current_word->ext_symbol_index = -1;

    printf("DEBUG: After encoding %s: current_ic = %d\n", inst->name, *current_ic);
    
    /* Encode operands */
    if (inst->num_operands >= 1) {
        MemoryWord operand_word, next_operand_word;
        
        /* Encode source operand */
        if (!encode_operand(operands[operand_start], symtab, &operand_word, FALSE, &next_operand_word))
            return FALSE;
        
        src_mode = (operands[operand_start][0] == IMMEDIATE_PREFIX) ? 
                  ADDR_MODE_IMMEDIATE : 
                  (strchr(operands[operand_start], LEFT_BRACKET_CHAR)) ? 
                  ADDR_MODE_MATRIX : 
                  (operands[operand_start][0] == REGISTER_CHAR && 
                   isdigit(operands[operand_start][1]) && 
                   operands[operand_start][2] == NULL_TERMINATOR) ? 
                  ADDR_MODE_REGISTER : 
                  ADDR_MODE_DIRECT;
        
        current_word->value |= (src_mode << SRC_SHIFT);
        
        /* Store additional operand words if needed */
        if (src_mode == ADDR_MODE_MATRIX || src_mode == ADDR_MODE_DIRECT || src_mode == ADDR_MODE_IMMEDIATE) {
            if (*current_ic >= WORD_COUNT) {
                print_error("Code section overflow", NULL);
                return FALSE;
            }
            memory->words[*current_ic++] = operand_word;
            
            /* For matrix addressing, store the second word with registers */
            if (src_mode == ADDR_MODE_MATRIX) {
                if (*current_ic >= WORD_COUNT) {
                    print_error("Code section overflow", NULL);
                    return FALSE;
                }
                memory->words[*current_ic++] = next_operand_word; /* This contains the register encoding */
            }
        }
        
        /* Encode destination operand */
        if (inst->num_operands >= 2) {
            MemoryWord dest_operand_word, dest_next_operand_word;
            
            if (!encode_operand(operands[operand_start + 1], symtab, &dest_operand_word, TRUE, &dest_next_operand_word))
                return FALSE;
            
            dest_mode = (operands[operand_start + 1][0] == REGISTER_CHAR && 
                        isdigit(operands[operand_start + 1][1]) && 
                        operands[operand_start + 1][2] == NULL_TERMINATOR) ? 
                       ADDR_MODE_REGISTER : 
                       (strchr(operands[operand_start + 1], LEFT_BRACKET_CHAR)) ?
                       ADDR_MODE_MATRIX :
                       ADDR_MODE_DIRECT;
            
            current_word->value |= (dest_mode << DEST_SHIFT);
            
            /* Store additional operand words if needed */
            if (dest_mode == ADDR_MODE_DIRECT || dest_mode == ADDR_MODE_MATRIX) {
                if (*current_ic >= WORD_COUNT) {
                    print_error("Code section overflow", NULL);
                    return FALSE;
                }
                memory->words[*current_ic++] = dest_operand_word;
                
                /* For matrix addressing, store the second word with registers */
                if (dest_mode == ADDR_MODE_MATRIX) {
                    if (*current_ic >= WORD_COUNT) {
                        print_error("Code section overflow", NULL);
                        return FALSE;
                    }
                    memory->words[*current_ic++] = dest_next_operand_word; /* This contains the register encoding */
                }
            }
        }
    }
    return TRUE;
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[10], dc_str[10];  /* Variable length for header */
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create object file", filename);
        return;
    }
    
    /* Write header with code and data sizes in base 4 */
    printf("DEBUG: Writing header - IC = %d, DC = %d\n", memory->ic, memory->dc);
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    printf("DEBUG: Converted to base4 - IC = %s, DC = %s\n", ic_str, dc_str);
    
    /* WRITE THE HEADER LINE! */
    fprintf(fp, "%s %s\n", ic_str, dc_str);
    
    /* Write instructions in base 4 */
    for (i = IC_START; i < IC_START + memory->ic; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    
    /* Write data in base 4 */
    for (i = IC_START + memory->ic; i < IC_START + memory->ic + memory->dc; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    fclose(fp);
}

static void write_entry_file(const char *filename, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create entry file", filename);
        return;
    }
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) {
            convert_to_base4_address(symtab->symbols[i].value, addr_str);
            fprintf(fp, "%s %s\n", symtab->symbols[i].name, addr_str);
        }
    }
    fclose(fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int j;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create extern file", filename);
        return;
    }
    
    /* Look through all memory words for external references */
    for (j = IC_START; j < IC_START + memory->ic; j++) {
        if (memory->words[j].are == ARE_EXTERNAL && memory->words[j].ext_symbol_index >= 0) {
            convert_to_base4_address(j, addr_str);
            fprintf(fp, "%s %s\n", 
                    symtab->symbols[memory->words[j].ext_symbol_index].name, 
                    addr_str);
        }
    }
    fclose(fp);
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory,
                const char *obj_file, const char *ent_file, const char *ext_file) {
    char line[MAX_LINE_LENGTH];
    int line_num = 0, error_flag = 0, current_ic = 0;
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    while (fgets(line, sizeof(line), fp)) {
        char **tokens = NULL;
        int i;
        int has_label = 0, token_count = 0;
        
        line_num++;
        trim_whitespace(line);
        
        if (line[0] == NULL_TERMINATOR || line[0] == COMMENT_CHAR)
            continue;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error\n", line_num);
            error_flag = 1;
            continue;
        }

        /* Check for label */
        for (i = 0; i < token_count; i++) {
            if (strchr(tokens[i], LABEL_TERMINATOR)) {
                has_label = 1;
                break;
            }
        }

        /* Process line */
        if (tokens[has_label ? 1 : 0][0] == DIRECTIVE_CHAR) {
            if (!process_directive(tokens + (has_label ? 1 : 0), 
                                             token_count - (has_label ? 1 : 0), 
                                             symtab, memory)) {
                fprintf(stderr, "Line %d: Directive error\n", line_num);
                error_flag = 1;
            }
        } else {
            const Instruction *inst = get_instruction(tokens[has_label ? 1 : 0]);

            if (inst) {
                if (!encode_instruction(inst, tokens + (has_label ? 2 : 1), symtab, memory, &current_ic)) {
                    fprintf(stderr, "Line %d: Encoding error\n", line_num);
                    error_flag = 1;
                }
            } else {
                fprintf(stderr, "Line %d: Unknown instruction\n", line_num);
                error_flag = 1;
            }
        }
        free_tokens(tokens, token_count);
    }
    fclose(fp);

    /* Only write files if no errors */
    if (!error_flag) {
        write_object_file(obj_file, memory);
    
        /* Only create .ent file if there are entry symbols */
        if (has_entries(symtab))
            write_entry_file(ent_file, symtab);
    
        /* In second_pass, before writing files: */
        printf("DEBUG: has_externs = %d, has_external_references = %d\n", has_externs(symtab), has_external_references(memory));

        /* Only create .ext file if there are external references */
        if (has_externs(symtab) && has_external_references(memory))
            write_extern_file(ext_file, memory, symtab);
    }
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}