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
#include "line_process.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int validate_operand_count(const Instruction *inst, int operand_count, const char *inst_name) {
    if (operand_count != inst->num_operands) {
        fprintf(stderr, "%d / %d", inst->num_operands, operand_count);
        return FALSE;
    }
    return TRUE;
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory) {
    int instruction_index, operand_start, operand_count, inst_length;
    int has_label = has_label_in_tokens(tokens, token_count);
    const Instruction *inst;
    
    if (has_label) {
        if (!process_label(tokens[0], symbol_table, INSTRUCTION_START + memory->ic, FALSE))
            return FALSE;

        instruction_index = 1;
    } else
        instruction_index = 0;
    
    /* Validate instruction */
    inst = get_instruction(tokens[instruction_index]);

    if (!inst) {
        print_error("Unknown instruction", tokens[instruction_index]);
        return FALSE;
    }

    /* Calculate and validate operands */
    operand_start = instruction_index + 1;
    operand_count = token_count - operand_start;

    if (!validate_operand_count(inst, operand_count, tokens[instruction_index]))
        return FALSE;
    
    inst_length = calculate_instruction_length(inst, tokens + operand_start, operand_count);

    if (inst_length == -1 || !validate_ic_limit(memory->ic + inst_length))
        return FALSE;

    memory->ic += inst_length;
    
    return TRUE;
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory) {
    int directive_index;
    int has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label) {
        if (!process_label(tokens[0], symbol_table, memory->dc, TRUE))
            return FALSE;

        directive_index = 1;
    } else
        directive_index = 0;
    
    return process_directive(tokens + directive_index, token_count - directive_index, symbol_table, memory, FALSE);
}

static void update_data_symbols(SymbolTable *symbol_table, int final_ic) {
    int i;
    
    for (i = 0; i < symbol_table->count; i++) {
        if (symbol_table->symbols[i].type == DATA_SYMBOL)
            symbol_table->symbols[i].value += INSTRUCTION_START + final_ic;
    }
}

static int process_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int line_num) {
    if (!validate_line_format(tokens, token_count)) {
        fprintf(stderr, "Line %d: Invalid line format \n", line_num);
        return FALSE;
    }

    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symbol_table, memory)) {
            fprintf(stderr, "Line %d: Directive error \n", line_num);
            return FALSE;
        }
    } else { /* Must be instruction line */
        if (!process_instruction_line(tokens, token_count, symbol_table, memory)) {
            fprintf(stderr, "Line %d: Instruction error \n", line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symbol_table, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error \n", line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        if (!process_line(tokens, token_count, symbol_table, memory, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    return error_flag ? FALSE : TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int first_pass(const char *filename, SymbolTable *symbol_table, MemoryImage *memory) {
    FILE *fp = NULL;

    fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(fp, symbol_table, memory)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }
    safe_fclose(&fp);
    update_data_symbols(symbol_table, memory->ic);
    printf("DEBUG: Final IC = %d, DC = %d \n", memory->ic, memory->dc);

    return TRUE;
}