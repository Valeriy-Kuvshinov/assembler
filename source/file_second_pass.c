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
        print_line_error("Invalid line format", NULL, line_num);
        return FALSE;
    }

    /* Check if it's a directive line */
    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symbol_table, memory)) {
            print_line_error("Directive error", NULL, line_num);
            return FALSE;
        }
    } else { /* Must be an instruction line */
        if (!process_instruction_line(tokens, token_count, symbol_table, memory, current_ic_ptr)) {
            print_line_error("Encoding error", NULL, line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symbol_table, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0, second_pass_ic = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Parse the line into tokens */
        if (!parse_tokens(line, &tokens, &token_count)) {
            print_line_error("Syntax error", NULL, line_num);
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

static void write_instruction_lines(FILE *fp, MemoryImage *memory) {
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;

    for (i = 0; i < memory->ic; i++) {
        int current_address = IC_START + i;

        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[current_address].raw, value_str);
        fprintf(fp, "%s %s%c", addr_str, value_str, NEWLINE);
    }
}

static void write_data_lines(FILE *fp, MemoryImage *memory) {
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;

    for (i = 0; i < memory->dc; i++) {
        int current_address = IC_START + memory->ic + i;

        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[i].raw, value_str);
        fprintf(fp, "%s %s%c", addr_str, value_str, NEWLINE);
    }
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[5], dc_str[5];
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    /* Write header */
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    fprintf(fp, "%s %s%c", ic_str, dc_str, NEWLINE);
    
    write_instruction_lines(fp, memory);
    write_data_lines(fp, memory);
    
    safe_fclose(&fp);
}

static void write_entry_file(const char *filename, SymbolTable *symbol_table) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (i = 0; i < symbol_table->count; i++) {
        if (symbol_table->symbols[i].is_entry) {
            convert_to_base4_address(symbol_table->symbols[i].value, addr_str);
            fprintf(fp, "%s %s%c", symbol_table->symbols[i].name, addr_str, NEWLINE);
        }
    }
    safe_fclose(&fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symbol_table) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    for (i = IC_START; i < IC_START + memory->ic; i++) {
        int ext_index = memory->words[i].operand.ext_symbol_index;

        if (memory->words[i].operand.are == ARE_EXTERNAL && ext_index >= 0) {
            const char *symbol_name = symbol_table->symbols[ext_index].name;

            convert_to_base4_address(i, addr_str);
            fprintf(fp, "%s %s%c", symbol_name, addr_str, NEWLINE);
        }
    }
    safe_fclose(&fp);
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