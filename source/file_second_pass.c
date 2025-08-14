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
    return has_label_in_tokens(tokens, token_count) ? 1 : 0; /* Determine the index of an instruction / directive */
}

static int encode_instruction(const Instruction *inst, char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr) {
    int operand_start_index, operand_count;
    MemoryWord *instruction_word;

    /* Parse operands to get their starting index and count */
    if (!parse_operands(tokens, token_count, &operand_start_index, &operand_count))
        return FALSE;

    /* Validate addressing modes for the instruction's operands */
    if (!check_operands(inst, tokens + operand_start_index, operand_count)) 
        return FALSE;

    /* Encode the first word of the instruction (opcode, ARE, addressing modes) */
    if (!encode_instruction_word(inst, memory, current_ic_ptr, &instruction_word))
        return FALSE;

    /* Encode the subsequent operand words */
    return encode_operands(inst, tokens, operand_start_index, operand_count, symtab, memory, current_ic_ptr, instruction_word);
}

static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr) {
    int inst_index;
    const Instruction *inst;

    inst_index = get_item_index(tokens, token_count);
    inst = get_instruction(tokens[inst_index]);

    if (!inst) {
        print_error("Unknown instruction", tokens[inst_index]);
        return FALSE;
    }
    return encode_instruction(inst, tokens, token_count, symtab, memory, current_ic_ptr);
}

static int process_directive_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory) {
    int direct_index;
    
    direct_index = get_item_index(tokens, token_count);
    
    /* Check if there's actually a directive token after a potential label */
    if (direct_index >= token_count) {
        print_error("Missing operand", "Missing directive after label");
        return FALSE;
    }
    return process_directive(tokens + direct_index, token_count - direct_index, symtab, memory, TRUE);
}

static int process_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, int line_num) {
    if (!check_line_format(tokens, token_count)) {
        print_line_error("Invalid line format", NULL, line_num);
        return FALSE;
    }

    /* Check if it's a directive line */
    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symtab, memory)) {
            print_line_error("Directive error", NULL, line_num);
            return FALSE;
        }
    } else { /* Must be an instruction line */
        if (!process_instruction_line(tokens, token_count, symtab, memory, current_ic_ptr)) {
            print_line_error("Encoding error", NULL, line_num);
            return FALSE;
        }
    }
    return TRUE;
}

static int process_file_lines(FILE *fp, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0, second_pass_ic = 0;

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

        if (!process_line(tokens, token_count, symtab, memory, &second_pass_ic, line_num))
            error_flag = 1;

        free_tokens(tokens, token_count);
    }
    return error_flag ? FALSE : TRUE;
}

static void write_instruction_lines(FILE *fp, MemoryImage *memory) {
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i, current_address;

    for (i = 0; i < memory->ic; i++) {
        current_address = IC_START + i;

        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[current_address].raw, value_str);
        write_file_line(fp, addr_str, value_str);
    }
}

static void write_data_lines(FILE *fp, MemoryImage *memory) {
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i, current_address;

    for (i = 0; i < memory->dc; i++) {
        current_address = IC_START + memory->ic + i;

        convert_to_base4_address(current_address, addr_str);
        convert_to_base4_word(memory->words[i].raw, value_str);
        write_file_line(fp, addr_str, value_str);
    }
}

static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[6], dc_str[6];
    FILE *fp = open_output_file(filename);
    
    if (!fp)
        return;
    
    /* Write header */
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    write_file_line(fp, ic_str, dc_str);

    write_instruction_lines(fp, memory);
    write_data_lines(fp, memory);
    
    safe_fclose(&fp);
}

static void write_entry_file(const char *filename, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = open_output_file(filename);

    if (!fp)
        return;

    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) {
            convert_to_base4_address(symtab->symbols[i].value, addr_str);
            write_file_line(fp, symtab->symbols[i].name, addr_str);
        }
    }
    safe_fclose(&fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    const char *symbol_name;
    char addr_str[ADDR_LENGTH];
    int i, ext_index;
    FILE *fp = open_output_file(filename);

    if (!fp)
        return;

    for (i = IC_START; i < IC_START + memory->ic; i++) {
        ext_index = memory->words[i].operand.ext_symbol_index;

        if ((memory->words[i].operand.are == ARE_EXTERNAL) && (ext_index >= 0)) {
            symbol_name = symtab->symbols[ext_index].name;

            convert_to_base4_address(i, addr_str);
            write_file_line(fp, symbol_name, addr_str);
        }
    }
    safe_fclose(&fp);
}

/* Outer methods */
/* ==================================================================== */
int second_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory, const char *obj_file, const char *ent_file, const char *ext_file) {
    FILE *fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(fp, symtab, memory)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    safe_fclose(&fp);
    write_object_file(obj_file, memory);

    if (has_entries(symtab))
        write_entry_file(ent_file, symtab);

    if (has_externs(symtab))
        write_extern_file(ext_file, memory, symtab);

    return TRUE;
}