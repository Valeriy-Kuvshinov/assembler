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
#include "encoder.h"
#include "line_process.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int get_item_index(char **tokens, int token_count) {
    /* Determine the index of an instruction / directive (skip label if present) */
    return has_label_in_tokens(tokens, token_count) ? 1 : 0;
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr) {
    int inst_idx;
    const Instruction *inst;

    inst_idx = get_item_index(tokens, token_count);
    inst = get_instruction(tokens[inst_idx]);

    if (!inst) {
        print_error("Unknown instruction", tokens[inst_idx]);
        return FALSE;
    }
    /* Encode the instruction and its operands into memory */
    return encode_instruction(inst, tokens, token_count, symbol_table, memory, current_ic_ptr);
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory) {
    int dir_idx;
    
    dir_idx = get_item_index(tokens, token_count);
    
    /* Check if there's actually a directive token after a potential label */
    if (dir_idx >= token_count) {
        print_error("Missing operand", "Missing directive after label");
        return FALSE;
    }
    return process_directive(tokens + dir_idx, token_count - dir_idx, symbol_table, memory, TRUE);
}

static int process_line(char **tokens, int token_count, SymbolTable *symbol_table, MemoryImage *memory, int *current_ic_ptr, int line_num) {
    if (!validate_line_format(tokens, token_count)) {
        fprintf(stderr, "Line %d: Invalid line format %c", line_num, NEWLINE);
        return FALSE;
    }

    /* Check if it's a directive line */
    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symbol_table, memory)) {
            fprintf(stderr, "Line %d: Directive error %c", line_num, NEWLINE);
            return FALSE;
        }
    } else { /* Must be an instruction line */
        if (!process_instruction_line(tokens, token_count, symbol_table, memory, current_ic_ptr)) {
            fprintf(stderr, "Line %d: Encoding error %c", line_num, NEWLINE);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symbol_table, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0;
    int second_pass_ic = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Parse the line into tokens */
        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error%c", line_num, NEWLINE);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        if (!process_line(tokens, token_count, symbol_table, memory, &second_pass_ic, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    return error_flag ? FALSE : TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symbol_table, MemoryImage *memory, const char *obj_file, const char *ent_file, const char *ext_file) {
    FILE *fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(fp, symbol_table, memory)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    safe_fclose(&fp);
    write_object_file(obj_file, memory);

    if (has_entries(symbol_table))
        write_entry_file(ent_file, symbol_table);

    if (has_externs(symbol_table))
        write_extern_file(ext_file, memory, symbol_table);

    return TRUE;
}