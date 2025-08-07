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
#include "line_process.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void init_memory(MemoryImage *memory) {
    int i;

    memory->ic = 0;
    memory->dc = 0;
    
    for (i = 0; i < MAX_WORD_COUNT; i++) {
        memory->words[i].value = 0;
        memory->words[i].are = ARE_ABSOLUTE;
        memory->words[i].ext_symbol_index = -1;
    }
}

static int init_pass(SymbolTable *symtab, MemoryImage *memory) {
    init_memory(memory);
    
    if (!init_symbol_table(symtab)) {
        print_error("Failed to initialize symbol table", NULL);
        return FALSE;
    }
    return TRUE;
}

static int validate_operand_count(const Instruction *inst, int operand_count, const char *inst_name) {
    if (operand_count != inst->num_operands) {
        fprintf(stderr, "%d / %d", inst->num_operands, operand_count);
        return FALSE;
    }
    return TRUE;
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int instruction_index, operand_start, operand_count, inst_length;
    const Instruction *inst;
    int has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label) {
        if (!process_label(tokens[0], symtab, INSTRUCTION_START + memory->ic, FALSE))
            return FALSE;

        instruction_index = 1;
    } else
        instruction_index = 0;
    
    /* validate instruction */
    inst = get_instruction(tokens[instruction_index]);

    if (!inst) {
        print_error(ERR_UNKNOWN_INSTRUCTION, tokens[instruction_index]);
        return FALSE;
    }

    /* Calculate and validate operands */
    operand_start = instruction_index + 1;
    operand_count = token_count - operand_start;

    printf("Instruction: %s, Expected operands: %d, Got: %d \n", 
           tokens[instruction_index], inst->num_operands, operand_count);

    if (!validate_operand_count(inst, operand_count, tokens[instruction_index]))
        return FALSE;
    
    inst_length = calc_instruction_length(inst, tokens + operand_start, operand_count);
    memory->ic += inst_length;
    
    return TRUE;
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int directive_index;
    int has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label) {
        if (!process_label(tokens[0], symtab, memory->dc, TRUE))
            return FALSE;

        directive_index = 1;
    } else
        directive_index = 0;
    
    return process_directive(tokens + directive_index, token_count - directive_index, symtab, memory, FALSE);
}

static void update_data_symbol_addresses(SymbolTable *symtab, int final_ic) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL)
            symtab->symbols[i].value += INSTRUCTION_START + final_ic;
    }
}

static int process_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int line_num) {
    if (!validate_line_format(tokens, token_count)) {
        fprintf(stderr, "Line %d: Invalid line format \n", line_num);
        return FALSE;
    }

    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symtab, memory)) {
            fprintf(stderr, "Line %d: Directive error \n", line_num);
            return FALSE;
        }
    } else { /* Must be instruction line */
        if (!process_instruction_line(tokens, token_count, symtab, memory)) {
            fprintf(stderr, "Line %d: Instruction error \n", line_num);
            return FALSE;
        }
    }
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0;
    FILE *fp = open_source_file(filename);
    
    if (!fp)
        return PASS_ERROR;

    if(!init_pass(symtab, memory))
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

        if (!process_line(tokens, token_count, symtab, memory, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    safe_fclose(&fp);
    
    update_data_symbol_addresses(symtab, memory->ic);
    printf("DEBUG: Final IC = %d, DC = %d \n", memory->ic, memory->dc);

    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}