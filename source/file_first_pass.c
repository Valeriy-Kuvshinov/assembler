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
#include "label.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void init_first_pass(SymbolTable *symtab, MemoryImage *memory) {
    int i;
    
    /* Initialize counters */
    memory->ic = 0;  
    memory->dc = 0;
    symtab->count = 0;

    /* Initialize memory words */
    for (i = 0; i < WORD_COUNT; i++) {
        memory->words[i].value = 0;
        memory->words[i].are = ARE_ABSOLUTE;
        memory->words[i].ext_symbol_index = -1;
    }
}

static int has_label_in_tokens(char **tokens) {
    return (strchr(tokens[0], LABEL_TERMINATOR) != NULL);
}

static int is_directive_line(char **tokens, int directive_idx, int token_count) {
    return (directive_idx < token_count && tokens[directive_idx][0] == DIRECTIVE_CHAR);
}

static int validate_operand_count(const Instruction *inst, int operand_count, const char *inst_name) {
    if (operand_count != inst->num_operands) {
        char error_msg[100];
        sprintf(error_msg, "Expected %d operands, got %d", inst->num_operands, operand_count);
        print_error(ERR_WRONG_OPERAND_COUNT, error_msg);

        return FALSE;
    }
    return TRUE;
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int operand_start, operand_count, inst_length;
    int inst_idx = 0;
    const Instruction *inst;
    
    if (token_count < 1) 
        return FALSE;
    
    /* Check for label */
    if (has_label_in_tokens(tokens)) {
        if (!process_label(tokens[0], symtab, INSTRUCTION_START + memory->ic, FALSE))
            return FALSE;

        inst_idx = 1;
    }
    
    if (inst_idx >= token_count) {
        print_error("Missing instruction after label", NULL);
        return FALSE;
    }
    
    inst = get_instruction(tokens[inst_idx]);

    if (!inst) {
        print_error(ERR_UNKNOWN_INSTRUCTION, tokens[inst_idx]);
        return FALSE;
    }

    /* Calculate operand count */
    operand_start = inst_idx + 1;
    operand_count = token_count - operand_start;

    printf("Instruction: %s, Expected operands: %d, Got: %d\n", tokens[inst_idx], inst->num_operands, operand_count);

    if (!validate_operand_count(inst, operand_count, tokens[inst_idx]))
        return FALSE;
    
    inst_length = calc_instruction_length(inst, tokens + operand_start, operand_count);
    memory->ic += inst_length; /* Update instruction counter */
    
    return TRUE;
}

static int process_single_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int line_num) {
    int has_label = 0, directive_idx = 0;
    int success = TRUE;
    
    /* Check for label */
    if (has_label_in_tokens(tokens)) {
        has_label = 1;
        directive_idx = 1;
    }

    /* Process line based on type */
    if (is_directive_line(tokens, directive_idx, token_count)) {
        /* Handle label for data directive - assign current DC value */
        if (has_label) {
            if (!process_label(tokens[0], symtab, memory->dc, TRUE)) {
                fprintf(stderr, "Line %d: Label error\n", line_num);
                success = FALSE;
            }
        }
        
        /* Check bounds and process directive */
        if (directive_idx >= token_count) {
            fprintf(stderr, "Line %d: Missing directive\n", line_num);
            success = FALSE;
        } else {
            if (!process_directive(tokens + directive_idx, token_count - directive_idx, symtab, memory, FALSE)) {
                fprintf(stderr, "Line %d: Directive error\n", line_num);
                success = FALSE;
            }
        }
    } else {
        /* Handle instruction - label gets current IC value */
        if (!process_instruction_line(tokens, token_count, symtab, memory)) {
            fprintf(stderr, "Line %d: Instruction error\n", line_num);
            success = FALSE;
        }
    }
    return success;
}

static void update_data_symbol_addresses(SymbolTable *symtab, int final_ic) {
    int i;
    
    /* CRITICAL: Update data symbol addresses to come after code section */
    /* Data symbols get: INSTRUCTION_START + final_IC + their_DC_offset */
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL)
            symtab->symbols[i].value += INSTRUCTION_START + final_ic;
    }
}

static int process_line(const char *line, SymbolTable *symtab, MemoryImage *memory, int line_num) {
    char **tokens = NULL;
    int token_count = 0;
    int success = TRUE;
    
    if (!parse_tokens(line, &tokens, &token_count)) {
        fprintf(stderr, "Line %d: Syntax error\n", line_num);
        return FALSE;
    }

    if (token_count > 0)
        success = process_single_line(tokens, token_count, symtab, memory, line_num);
    
    free_tokens(tokens, token_count);

    return success;
}

/* Outer regular methods */
/* ==================================================================== */
int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    int line_num = 0, error_flag = 0;
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    init_first_pass(symtab, memory);

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        trim_whitespace(line);
        
        /* Skip empty/comments */
        if (should_skip_line(line))
            continue;

        if (!process_line(line, symtab, memory, line_num))
            error_flag = 1;
    }
    fclose(fp);
    
    update_data_symbol_addresses(symtab, memory->ic);

    /* At the end of first_pass, before returning */
    printf("DEBUG: Final IC = %d, DC = %d\n", memory->ic, memory->dc);

    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}