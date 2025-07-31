#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "file_passer.h"
#include "tokenizer.h"
#include "instructions.h"
#include "errors.h"
#include "utils.h"

static int is_valid_label(const char *label) {
    int i;
    
    /* Basic validation */
    if (!label || !*label || strlen(label) >= MAX_LABEL_LENGTH)
        return FALSE;
    
    /* First character must be alphabetic */
    if (!isalpha((unsigned char)label[0]))
        return FALSE;
        
    /* Subsequent characters must be alphanumeric */
    for (i = 1; label[i]; i++) {
        if (!isalnum((unsigned char)label[i]))
            return FALSE;
    }
    
    return TRUE;
}

static int add_symbol(SymbolTable *symtab, const char *name, int value, int type) {
    int i;
    
    /* Check for duplicates */
    for (i = 0; i < symtab->count; i++) {
        if (strcmp(symtab->symbols[i].name, name) == 0) {
            print_error(ERR_DUPLICATE_LABEL, name);
            return FALSE;
        }
    }
    
    if (symtab->count >= MAX_LABELS) {
        print_error("Symbol table overflow", NULL);
        return FALSE;
    }
    
    /* Add new symbol */
    strncpy(symtab->symbols[symtab->count].name, name, MAX_LABEL_LENGTH - 1);

    symtab->symbols[symtab->count].name[MAX_LABEL_LENGTH - 1] = '\0';
    symtab->symbols[symtab->count].value = value;
    symtab->symbols[symtab->count].type = type;
    symtab->symbols[symtab->count].is_entry = FALSE;
    symtab->symbols[symtab->count].is_extern = FALSE;
    symtab->count++;
    
    return TRUE;
}

static int process_label(char *label, SymbolTable *symtab, int address, int is_data) {
    char *colon = strchr(label, ':'); /* Remove trailing colon */

    if (colon) 
        *colon = '\0';
    
    if (!is_valid_label(label)) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    return add_symbol(symtab, label, address, is_data ? DATA_SYMBOL : CODE_SYMBOL);
}

static int process_data_directive(char **tokens, int token_count, int start_idx,
                                 SymbolTable *symtab, MemoryImage *memory) {
    int i, value;
    char *endptr;
    
    if (token_count <= start_idx + 1) {
        print_error("Missing data values", NULL);
        return FALSE;
    }
    
    for (i = start_idx + 1; i < token_count; i++) {
        value = strtol(tokens[i], &endptr, 10);

        if (*endptr != '\0') {
            print_error("Invalid data value", tokens[i]);
            return FALSE;
        }
        
        /* Store data word */
        if (memory->dc >= WORD_COUNT) {
            print_error("Data section overflow", NULL);
            return FALSE;
        }
        
        /* Actually store the value */
        memory->words[IC_START + memory->ic + memory->dc].value = value & WORD_MASK;
        memory->dc++;
    }
    return TRUE;
}

static int process_string_directive(char **tokens, int token_count, int start_idx,
                                   SymbolTable *symtab, MemoryImage *memory) {
    int i;
    char *str;
    
    if (token_count != start_idx + 2) {
        print_error("Invalid string directive", NULL);
        return FALSE;
    }
    
    str = tokens[start_idx + 1];
    
    /* Store each character (excluding quotes) */
    for (i = 0; str[i]; i++) {
        if (memory->dc >= WORD_COUNT) {
            print_error("Data section overflow", NULL);
            return FALSE;
        }
        memory->words[IC_START + memory->ic + memory->dc].value = (unsigned char)str[i];
        memory->words[IC_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
        memory->dc++;
    }
    
    /* Add null terminator */
    if (memory->dc >= WORD_COUNT) {
        print_error("Data section overflow", NULL);
        return FALSE;
    }
    memory->words[IC_START + memory->ic + memory->dc].value = 0;
    memory->words[IC_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
    memory->dc++;
    
    return TRUE;
}

static int process_mat_directive(char **tokens, int token_count, int start_idx,
                                SymbolTable *symtab, MemoryImage *memory) {
    int rows, cols, i;
    char *matrix_def;
    
    /* Must have at least .mat [rows][cols] and one value */
    if (token_count <= start_idx + 2) {
        print_error("Invalid matrix directive", NULL);
        return FALSE;
    }
    
    matrix_def = tokens[start_idx + 1];
    
    /* Parse [rows][cols] */
    if (sscanf(matrix_def, "[%d][%d]", &rows, &cols) != 2) {
        print_error("Invalid matrix dimensions", matrix_def);
        return FALSE;
    }
    
    /* Validate dimensions */
    if (rows <= 0 || cols <= 0) {
        print_error("Invalid matrix dimensions", matrix_def);
        return FALSE;
    }
    
    /* Validate number of values */
    if (token_count != start_idx + 2 + (rows * cols)) {
        print_error("Incorrect number of matrix values", NULL);
        return FALSE;
    }
    
    /* Store values */
    for (i = 0; i < rows * cols; i++) {
        char *endptr;
        int value = strtol(tokens[start_idx + 2 + i], &endptr, 10);
        
        if (*endptr != '\0') {
            print_error("Invalid matrix value", tokens[start_idx + 2 + i]);
            return FALSE;
        }
        
        if (memory->dc >= WORD_COUNT) {
            print_error("Data section overflow", NULL);
            return FALSE;
        }
        
        memory->words[IC_START + memory->ic + memory->dc].value = value & WORD_MASK;
        memory->dc++;
    }
    return TRUE;
}

static int process_directive(char **tokens, int token_count, int start_idx,
                            SymbolTable *symtab, MemoryImage *memory) {
    if (start_idx >= token_count) return FALSE;
    
    if (strcmp(tokens[start_idx], ".data") == 0) 
        return process_data_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], ".string") == 0) 
        return process_string_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], ".mat") == 0) 
        return process_mat_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], ".entry") == 0) {
        /* Handled in second pass */
        return TRUE;
    }
    else if (strcmp(tokens[start_idx], ".extern") == 0) {
        if (token_count != start_idx + 2) {
            print_error("Invalid extern directive", NULL);
            return FALSE;
        }
        return add_symbol(symtab, tokens[start_idx + 1], 0, EXTERNAL_SYMBOL);
    }
    
    print_error("Unknown directive", tokens[start_idx]);

    return FALSE;
}

static int calculate_instruction_length(const Instruction *inst, char **operands, int operand_count) {
    int i, length = 1; /* Base instruction word */
    
    for (i = 0; i < operand_count; i++) {
        /* Check addressing mode and add words accordingly */
        if (operands[i][0] == '#') {
            /* Immediate - needs extra word */
            length++;
        }
        else if (strchr(operands[i], '[')) {
            /* Matrix - needs 2 extra words */
            length += 2;
        }
        else if (operands[i][0] == 'r' && isdigit(operands[i][1]) && operands[i][2] == '\0') {
            /* Register - no extra word needed */
        }
        else {
            /* Direct addressing - needs extra word */
            length++;
        }
    }
    return length;
}

static int process_instruction(char **tokens, int token_count, 
                              SymbolTable *symtab, MemoryImage *memory) {
    int operand_start, operand_count, inst_length, inst_idx = 0;
    const Instruction *inst;
    
    if (token_count < 1) return FALSE;
    
    /* Check for label */
    if (strchr(tokens[0], ':')) {
        if (!process_label(tokens[0], symtab, IC_START + memory->ic, FALSE))
            return FALSE;
        inst_idx = 1;
    }
    
    if (inst_idx >= token_count) {
        print_error("Missing instruction after label", NULL);
        return FALSE;
    }
    
    /* Find instruction */
    inst = get_instruction(tokens[inst_idx]);
    if (!inst) {
        print_error(ERR_UNDEFINED_INSTRUCTION, tokens[inst_idx]);
        return FALSE;
    }
    
    /* Calculate operand count */
    operand_start = inst_idx + 1;
    operand_count = token_count - operand_start;
    
    /* Debug print */
    printf("Instruction: %s, Expected operands: %d, Got: %d\n", tokens[inst_idx], inst->num_operands, operand_count);

    /* Validate operand count */
    if (operand_count != inst->num_operands) {
        char error_msg[100];
        sprintf(error_msg, "Expected %d operands, got %d", inst->num_operands, operand_count);
        print_error(ERR_INVALID_OPERAND_COUNT, error_msg);
        return FALSE;
    }
    
    /* Calculate instruction length */
    inst_length = calculate_instruction_length(inst, tokens + operand_start, operand_count);
    
    /* Update instruction counter */
    memory->ic += inst_length;
    
    return TRUE;
}

/* First pass implementation */
int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory) {
    int i, line_num = 0, error_flag = 0;
    char line[MAX_LINE_LENGTH];
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    /* Initialize */
    memory->ic = 0;  /* Relative to IC_START */
    memory->dc = 0;
    symtab->count = 0;

    while (fgets(line, sizeof(line), fp)) {
        char **tokens = NULL;
        int token_count = 0;
        int has_label = 0;
        int directive_idx = 0;
        
        line_num++;
        trim_whitespace(line);
        
        /* Skip empty/comments */
        if (line[0] == '\0' || line[0] == COMMENT_CHAR)
            continue;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error\n", line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        /* Check for label */
        if (strchr(tokens[0], ':')) {
            has_label = 1;
            directive_idx = 1;
        }

        /* Process line */
        if (directive_idx < token_count && tokens[directive_idx][0] == DIRECTIVE_CHAR) {
            /* Handle label for data directive */
            if (has_label) {
                if (!process_label(tokens[0], symtab, memory->dc, TRUE)) {
                    fprintf(stderr, "Line %d: Label error\n", line_num);
                    error_flag = 1;
                }
            }
            
            if (!process_directive(tokens, token_count, directive_idx, symtab, memory)) {
                fprintf(stderr, "Line %d: Directive error\n", line_num);
                error_flag = 1;
            }
        }
        else {
            if (!process_instruction(tokens, token_count, symtab, memory)) {
                fprintf(stderr, "Line %d: Instruction error\n", line_num);
                error_flag = 1;
            }
        }
        free_tokens(tokens, token_count);
    }
    fclose(fp);
    
    /* Update data symbol addresses */
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL) {
            symtab->symbols[i].value += IC_START + memory->ic;
        }
    }
    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}