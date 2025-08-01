#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "errors.h"
#include "utils.h"
#include "memory_layout.h"
#include "instructions.h"
#include "file_passer.h"
#include "tokenizer.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void convert_to_base4_address(int value, char *result) {
    char digits[] = {BASE4_A_DIGIT, BASE4_B_DIGIT, BASE4_C_DIGIT, BASE4_D_DIGIT};
    int i;
    
    /* Convert to 4-digit base 4 for addresses */
    for (i = ADDR_LENGTH - 2; i >= 0; i--) {  /* ADDR_LENGTH-2 = 3 (indices 3,2,1,0) */
        result[i] = digits[value % BASE4_DIGITS];
        value /= BASE4_DIGITS;
    }
    result[ADDR_LENGTH - 1] = '\0';  /* ADDR_LENGTH-1 = 4 */
}

static void convert_to_base4_word(int value, char *result) {
    char digits[] = {BASE4_A_DIGIT, BASE4_B_DIGIT, BASE4_C_DIGIT, BASE4_D_DIGIT};
    int i;
    
    /* Convert to 5-digit base 4 for machine words */
    for (i = WORD_LENGTH - 2; i >= 0; i--) {  /* WORD_LENGTH-2 = 4 (indices 4,3,2,1,0) */
        result[i] = digits[value % BASE4_DIGITS];
        value /= BASE4_DIGITS;
    }
    result[WORD_LENGTH - 1] = '\0';  /* WORD_LENGTH-1 = 5 */
}

static int has_external_references(const MemoryImage *memory) {
    int i;
    
    for (i = IC_START; i < IC_START + memory->ic; i++) {
        if (memory->words[i].are == ARE_EXTERNAL)
            return TRUE;
    }
    return FALSE;
}

static int process_entry_directive(char **tokens, int token_count, SymbolTable *symtab) {
    int i;
    
    if (token_count != 2) {
        print_error("Invalid entry directive", NULL);
        return FALSE;
    }
    
    /* Find the symbol and mark it as entry */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, tokens[1]) == 0) {
            symtab->symbols[i].is_entry = TRUE;
            return TRUE;
        }
    }
    print_error(ERR_ENTRY_LABEL_NOT_FOUND, tokens[1]);

    return FALSE;
}

static int process_directive_second_pass(char **tokens, int token_count, 
                                       SymbolTable *symtab, MemoryImage *memory) {
    if (token_count < 1) 
        return FALSE;
    
    if (strcmp(tokens[0], ".entry") == 0) 
        return process_entry_directive(tokens, token_count, symtab);

    return TRUE;
}

static int parse_matrix_operand(const char *operand, int *base_reg, int *index_reg) {
    char temp[MAX_LINE_LENGTH];
    char *open1, *close1, *open2, *close2, *reg1_str, *reg2_str;
    
    /* Copy operand to temp buffer for manipulation */
    strncpy(temp, operand, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = '\0';
    
    /* Find brackets */
    open1 = strchr(temp, '[');

    if (!open1) 
        return FALSE;

    close1 = strchr(open1, ']');

    if (!close1) 
        return FALSE;
    
    open2 = strchr(close1, '[');
    if (!open2) 
        return FALSE;

    close2 = strchr(open2, ']');

    if (!close2) 
        return FALSE;
    
    /* Extract register numbers */
    *close1 = '\0';
    *close2 = '\0';
    reg1_str = open1 + 1;
    reg2_str = open2 + 1;
    
    /* Validate register format */
    if (reg1_str[0] != 'r' || reg2_str[0] != 'r') 
        return FALSE;
    
    *base_reg = atoi(reg1_str + 1);
    *index_reg = atoi(reg2_str + 1);
    
    if (*base_reg < 0 || *base_reg > MAX_REGISTER ||
        *index_reg < 0 || *index_reg > MAX_REGISTER)
        return FALSE;
    
    return TRUE;
}

static int encode_operand(const char *operand, SymbolTable *symtab, 
                         MemoryWord *word, int is_dest) {
    char *endptr;
    int i;
    long value;
    
    /* Immediate value (#num) */
    if (operand[0] == IMMEDIATE_PREFIX) {
        value = strtol(operand + 1, &endptr, 10);

        if (*endptr != '\0') {
            print_error("Invalid immediate value", operand);
            return FALSE;
        }
        word->value = value & WORD_MASK;
        word->are = ARE_ABSOLUTE;

        return TRUE;
    }
    
    /* Register (r0-r7) */
    if (operand[0] == 'r' && isdigit(operand[1]) && operand[2] == '\0') {
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
    if (strchr(operand, '[')) {
        char label[MAX_LABEL_LENGTH];
        char *bracket = strchr(operand, '[');
        int base_reg, index_reg;
        
        /* Extract label part */
        strncpy(label, operand, bracket - operand);
        label[bracket - operand] = '\0';
        
        /* Parse matrix registers */
        if (!parse_matrix_operand(operand, &base_reg, &index_reg)) {
            print_error("Invalid matrix addressing", operand);
            return FALSE;
        }
        
        /* Find label in symbol table */
        for (i = 0; i < symtab->count; i++) {
            if (strcmp(symtab->symbols[i].name, label) == 0) {
                word->value = symtab->symbols[i].value;
                word->are = (symtab->symbols[i].type == EXTERNAL_SYMBOL) ? 
                           ARE_EXTERNAL : ARE_RELOCATABLE;
                
                /* Store registers in next word */
                word[1].value = (base_reg << 6) | (index_reg << 2);
                word[1].are = ARE_ABSOLUTE;
                return TRUE;
            }
        }
        print_error(ERR_UNKNOWN_LABEL, label);

        return FALSE;
    }
    
    /* Symbol/label */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, operand) == 0) {
            if (symtab->symbols[i].type == EXTERNAL_SYMBOL) {
                word->value = 0; /* External symbols have value 0 */
                word->are = ARE_EXTERNAL;
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
        if (operands[i][0] == '#')
            addr_mode = ADDR_FLAG_IMMEDIATE;
        else if (strchr(operands[i], '['))
            addr_mode = ADDR_FLAG_MATRIX;
        else if (operands[i][0] == 'r' && isdigit(operands[i][1]) && operands[i][2] == '\0')
            addr_mode = ADDR_FLAG_REGISTER;
        else
            addr_mode = ADDR_FLAG_DIRECT;
        
        /* Check if addressing mode is legal for this operand position */
        legal_modes = (i == 0) ? inst->legal_src_addr_modes : inst->legal_dest_addr_modes;
        
        if (!(legal_modes & addr_mode)) {
            print_error(ERR_ILLEGAL_ADDRESSING_MODE, operands[i]);
            return FALSE;
        }
    }
    return TRUE;
}

static int encode_instruction(const Instruction *inst, char **operands, 
                             SymbolTable *symtab, MemoryImage *memory) {
    int opcode, i;
    int operand_start = 0, operand_count = 0, src_mode = 0, dest_mode = 0;
    MemoryWord *current_word;
    
    /* Check for label (should have been processed in first pass) */
    for (i = 0; operands[i]; i++) {
        if (strchr(operands[i], ':')) {
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
    
    /* Get current memory word */
    if (memory->ic >= WORD_COUNT) {
        print_error("Code section overflow", NULL);
        return FALSE;
    }
    current_word = &memory->words[memory->ic++];
    
    /* Encode opcode using the helper function */
    opcode = get_instruction_opcode(inst->name);

    if (opcode == -1) {
        print_error("Invalid instruction", inst->name);
        return FALSE;
    }
    current_word->value = opcode << OPCODE_SHIFT;
    current_word->are = ARE_ABSOLUTE;
    
    /* Encode operands */
    if (inst->num_operands >= 1) {
        MemoryWord operand_word;
        
        /* Encode source operand */
        if (!encode_operand(operands[operand_start], symtab, &operand_word, FALSE))
            return FALSE;
        
        src_mode = (operands[operand_start][0] == IMMEDIATE_PREFIX) ? 
                  ADDR_MODE_IMMEDIATE : 
                  (strchr(operands[operand_start], '[')) ? 
                  ADDR_MODE_MATRIX : 
                  (operands[operand_start][0] == 'r' && 
                   isdigit(operands[operand_start][1]) && 
                   operands[operand_start][2] == '\0') ? 
                  ADDR_MODE_REGISTER : 
                  ADDR_MODE_DIRECT;
        
        current_word->value |= (src_mode << SRC_SHIFT);
        
        /* Store additional operand words if needed */
        if (src_mode == ADDR_MODE_MATRIX || src_mode == ADDR_MODE_DIRECT || src_mode == ADDR_MODE_IMMEDIATE) {
            if (memory->ic >= WORD_COUNT) {
                print_error("Code section overflow", NULL);
                return FALSE;
            }
            memory->words[memory->ic++] = operand_word;
            
            /* For matrix addressing, store the second word with registers */
            if (src_mode == ADDR_MODE_MATRIX) {
                if (memory->ic >= WORD_COUNT) {
                    print_error("Code section overflow", NULL);
                    return FALSE;
                }
                memory->words[memory->ic++] = operand_word; /* This contains the register encoding */
            }
        }
        
        /* Encode destination operand */
        if (inst->num_operands >= 2) {
            if (!encode_operand(operands[operand_start + 1], symtab, &operand_word, TRUE))
                return FALSE;
            
            dest_mode = (operands[operand_start + 1][0] == 'r' && 
                        isdigit(operands[operand_start + 1][1]) && 
                        operands[operand_start + 1][2] == '\0') ? 
                       ADDR_MODE_REGISTER : 
                       (strchr(operands[operand_start + 1], '[')) ?
                       ADDR_MODE_MATRIX :
                       ADDR_MODE_DIRECT;
            
            current_word->value |= (dest_mode << DEST_SHIFT);
            
            /* Store additional operand words if needed */
            if (dest_mode == ADDR_MODE_DIRECT || dest_mode == ADDR_MODE_MATRIX) {
                if (memory->ic >= WORD_COUNT) {
                    print_error("Code section overflow", NULL);
                    return FALSE;
                }
                memory->words[memory->ic++] = operand_word;
                
                /* For matrix addressing, store the second word with registers */
                if (dest_mode == ADDR_MODE_MATRIX) {
                    if (memory->ic >= WORD_COUNT) {
                        print_error("Code section overflow", NULL);
                        return FALSE;
                    }
                    memory->words[memory->ic++] = operand_word; /* This contains the register encoding */
                }
            }
        }
    }
    return TRUE;
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create object file", filename);
        return;
    }
    
    /* Write header with code and data sizes in base 4 */
    convert_to_base4_word(memory->ic, addr_str);
    convert_to_base4_word(memory->dc, value_str);
    fprintf(fp, "%s %s\n", addr_str, value_str);
    
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
    int i, j;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create extern file", filename);
        return;
    }
    
    for (j = IC_START; j < IC_START + memory->ic; j++) {
        if (memory->words[j].are == ARE_EXTERNAL) {
            for (i = 0; i < symtab->count; i++) {
                if (symtab->symbols[i].type == EXTERNAL_SYMBOL) {
                    convert_to_base4_address(j, addr_str);
                    fprintf(fp, "%s %s\n", symtab->symbols[i].name, addr_str);
                    break;
                }
            }
        }
    }
    fclose(fp);
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory,
                const char *obj_file, const char *ent_file, const char *ext_file) {
    char line[MAX_LINE_LENGTH];
    int line_num = 0, error_flag = 0;
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    /* Reset counters */
    memory->ic = 0;  /* Will be offset by IC_START when writing */

    while (fgets(line, sizeof(line), fp)) {
        char **tokens = NULL;
        int i;
        int has_label = 0, token_count = 0;
        
        line_num++;
        trim_whitespace(line);
        
        if (line[0] == '\0' || line[0] == COMMENT_CHAR)
            continue;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error\n", line_num);
            error_flag = 1;
            continue;
        }

        /* Check for label */
        for (i = 0; i < token_count; i++) {
            if (strchr(tokens[i], ':')) {
                has_label = 1;
                break;
            }
        }

        /* Process line */
        if (tokens[has_label ? 1 : 0][0] == DIRECTIVE_CHAR) {
            if (!process_directive_second_pass(tokens + (has_label ? 1 : 0), 
                                             token_count - (has_label ? 1 : 0), 
                                             symtab, memory)) {
                fprintf(stderr, "Line %d: Directive error\n", line_num);
                error_flag = 1;
            }
        } else {
            const Instruction *inst = get_instruction(tokens[has_label ? 1 : 0]);

            if (inst) {
                if (!encode_instruction(inst, tokens + (has_label ? 2 : 1), symtab, memory)) {
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
    
        /* Only create .ext file if there are external references */
        if (has_externs(symtab) && has_external_references(memory))
            write_extern_file(ext_file, memory, symtab);
    }
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}