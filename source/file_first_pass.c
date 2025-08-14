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
static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    char **operands;
    int inst_index, operand_count, inst_length;
    const Instruction *inst;

    inst_index = has_label_in_tokens(tokens, token_count) ? 1 : 0;

    if ((inst_index == 1) && (!process_label(tokens[0], symtab, IC_START + memory->ic, FALSE)))
        return FALSE;

    inst = get_instruction(tokens[inst_index]);

    if (!inst) {
        print_error("Unknown instruction", tokens[inst_index]);
        return FALSE;
    }

    operands = tokens + inst_index + 1;
    operand_count = token_count - (inst_index + 1);
    inst_length = calculate_instruction_length(inst, operands, operand_count);

    if (inst_length == -1) 
        return FALSE;

    if (!check_ic_limit(memory->ic + inst_length))
        return FALSE;

    memory->ic += inst_length;

    return TRUE;
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int direct_index, has_label;

    has_label = has_label_in_tokens(tokens, token_count);
    
    if (has_label) {
        if (!process_label(tokens[0], symtab, memory->dc, TRUE))
            return FALSE;

        direct_index = 1;
    } else
        direct_index = 0;
    
    return process_directive(tokens + direct_index, token_count - direct_index, symtab, memory, FALSE);
}

static void update_data_symbols(SymbolTable *symtab, int final_ic) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL)
            symtab->symbols[i].value += IC_START + final_ic;
    }
}

static int process_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int line_num) {
    if (!check_line_format(tokens, token_count)) {
        print_line_error("Invalid line format", NULL, line_num);
        return FALSE;
    }

    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symtab, memory)) {
            print_line_error("Directive error", NULL, line_num);
            return FALSE;
        }
    } else { /* Must be instruction line */
        if (!process_instruction_line(tokens, token_count, symtab, memory)) {
            print_line_error("Instruction error", NULL, line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        if (!parse_tokens(line, &tokens, &token_count)) {
            print_line_error("Syntax error", NULL, line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        if (!process_line(tokens, token_count, symtab, memory, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    return error_flag ? FALSE : TRUE;
}

/* Outer methods */
/* ==================================================================== */
int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory) {
    FILE *fp = NULL;

    fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(fp, symtab, memory)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }
    safe_fclose(&fp);
    update_data_symbols(symtab, memory->ic);

    printf("%s: IC = %d, DC = %d\n", filename, memory->ic, memory->dc);

    return TRUE;
}