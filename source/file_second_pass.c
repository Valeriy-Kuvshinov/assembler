#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "file_passer.h"
#include "tokenizer.h"
#include "instructions.h"
#include "errors.h"
#include "utils.h"

/* Helper functions */
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
    char *open1, *close1, *open2, *close2, *reg1_str, *reg2_str;
    char temp[MAX_LINE_LENGTH];
    
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
    int i;
    char *endptr;
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
        int base_reg, index_reg;
        char label[MAX_LABEL_LENGTH];
        char *bracket = strchr(operand, '[');
        
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
            word->value = symtab->symbols[i].value;

            if (symtab->symbols[i].type == EXTERNAL_SYMBOL)
                word->are = ARE_EXTERNAL;
            else
                word->are = (symtab->symbols[i].type == DATA_SYMBOL) ? 
                           ARE_RELOCATABLE : ARE_ABSOLUTE;
            
            return TRUE;
        }
    }
    print_error(ERR_UNKNOWN_LABEL, operand);

    return FALSE;
}

static int encode_instruction(const Instruction *inst, char **operands, 
                             SymbolTable *symtab, MemoryImage *memory) {
    int opcode, i;
    int src_mode = 0, dest_mode = 0, has_label = 0;
    MemoryWord *current_word;
    
    /* Check for label (should have been processed in first pass) */
    for (i = 0; operands[i]; i++) {
        if (strchr(operands[i], ':')) {
            has_label = 1;
            break;
        }
    }
    
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
        if (!encode_operand(operands[has_label ? 1 : 0], symtab, &operand_word, FALSE))
            return FALSE;
        
        src_mode = (operands[has_label ? 1 : 0][0] == IMMEDIATE_PREFIX) ? 
                  ADDR_MODE_IMMEDIATE : 
                  (strchr(operands[has_label ? 1 : 0], '[')) ? 
                  ADDR_MODE_MATRIX : 
                  (operands[has_label ? 1 : 0][0] == 'r') ? 
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
        }
        
        /* Encode destination operand */
        if (inst->num_operands >= 2) {
            if (!encode_operand(operands[has_label ? 2 : 1], symtab, &operand_word, TRUE))
                return FALSE;
            
            dest_mode = (operands[has_label ? 2 : 1][0] == 'r') ? 
                       ADDR_MODE_REGISTER : 
                       ADDR_MODE_DIRECT;
            
            current_word->value |= (dest_mode << DEST_SHIFT);
            
            /* Store additional operand words if needed */
            if (dest_mode == ADDR_MODE_DIRECT) {
                if (memory->ic >= WORD_COUNT) {
                    print_error("Code section overflow", NULL);
                    return FALSE;
                }
                memory->words[memory->ic++] = operand_word;
            }
        }
    }
    return TRUE;
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create object file", filename);
        return;
    }
    
    /* Write header with code and data sizes */
    fprintf(fp, "%d %d\n", memory->ic, memory->dc);
    
    /* Write instructions */
    for (i = IC_START; i < IC_START + memory->ic; i++) {
        fprintf(fp, "%04d %04X\n", i, memory->words[i].value);
    }
    
    /* Write data */
    for (i = IC_START + memory->ic; i < IC_START + memory->ic + memory->dc; i++) {
        fprintf(fp, "%04d %04X\n", i, memory->words[i].value);
    }
    
    fclose(fp);
}

static void write_entry_file(const char *filename, SymbolTable *symtab) {
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create entry file", filename);
        return;
    }
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) {
            fprintf(fp, "%s %04d\n", symtab->symbols[i].name, symtab->symbols[i].value);
        }
    }
    fclose(fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    int i, j;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create extern file", filename);
        return;
    }
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == EXTERNAL_SYMBOL) {
            /* Find all references to this extern */
            for (j = IC_START; j < IC_START + memory->ic; j++) {
                if (memory->words[j].value == symtab->symbols[i].value && 
                    memory->words[j].are == ARE_EXTERNAL) {
                    fprintf(fp, "%s %04d\n", symtab->symbols[i].name, j);
                }
            }
        }
    }
    fclose(fp);
}

/* Second pass implementation */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory,
                const char *obj_file, const char *ent_file, const char *ext_file) {
    int line_num = 0, error_flag = 0;
    char line[MAX_LINE_LENGTH];
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    /* Reset counters */
    memory->ic = 0;  /* Will be offset by IC_START when writing */

    while (fgets(line, sizeof(line), fp)) {
        char **tokens = NULL;
        int token_count = 0;
        int i, has_label = 0;
        
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
            }
            else {
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
        if (has_entries(symtab))
            write_entry_file(ent_file, symtab);
        if (has_externs(symtab))
            write_extern_file(ext_file, memory, symtab);
    }
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}