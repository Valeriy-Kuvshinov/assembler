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
/*
Function to check the starting index of an instruction / directive in a token array.
Accounts for potential label at the beginning of the line.
Receives: char **tokens - Array of tokens from the line
          int token_count - Number of tokens in the array
Returns: int - Index where instruction/directive begins (0 or 1)
*/
static int get_item_index(char **tokens, int token_count) {
    return is_first_token_label(tokens, token_count) ? 1 : 0;
}

/*
Function to encode an instruction into memory during second pass.
Handles opcode, addressing modes, and operand parsing & encoding.
Receives: const Instruction *inst - Pointer to instruction definition
          char **tokens - Array of tokens from the line
          int token_count - Number of tokens in the array
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current instruction counter
Returns: int - TRUE if encoding succeeded, FALSE on error
*/
static int encode_instruction(const Instruction *inst, char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr) {
    int operand_start_index, operand_count;
    MemoryWord *instruction_word;

    if (!parse_operands(tokens, token_count, &operand_start_index, &operand_count))
        return FALSE;

    if (!check_operands(inst, tokens + operand_start_index, operand_count)) 
        return FALSE;

    if (!encode_instruction_word(inst, memory, current_ic_ptr, &instruction_word))
        return FALSE;

    return encode_operands(inst, tokens, operand_start_index, operand_count, symtab, memory, current_ic_ptr, instruction_word);
}

/*
Function to process a single instruction line during second pass.
Validates instruction and delegates to encoding function.
Receives: char **tokens - Array of tokens from the line
          int token_count - Number of tokens in the array
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current instruction counter
          int line_num - Current line number for error reporting
Returns: int - TRUE if processing succeeded, FALSE on error
*/
static int process_instruction_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, int line_num) {
    int inst_index;
    const Instruction *inst;

    inst_index = get_item_index(tokens, token_count);
    inst = get_instruction(tokens[inst_index]);

    if (!inst) {
        print_line_error("Unknown instruction", tokens[inst_index], line_num);
        return FALSE;
    }
    return encode_instruction(inst, tokens, token_count, symtab, memory, current_ic_ptr);
}

/*
Function to process a single directive line during second pass.
Handles data directives and symbol declarations.
Receives: char **tokens - Array of tokens from the line
          int token_count - Number of tokens in the array
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int line_num - Current line number for error reporting
Returns: int - TRUE if processing succeeded, FALSE on error
*/
static int process_directive_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int line_num) {
    int direct_index;
    
    direct_index = get_item_index(tokens, token_count);
    
    if (direct_index >= token_count) {
        print_line_error("Missing operand", "Missing directive after label", line_num);
        return FALSE;
    }
    return process_directive(tokens + direct_index, token_count - direct_index, symtab, memory, TRUE);
}

/*
Function to process a single line during second pass.
Determines if line is instruction or directive, and routes accordingly.
Receives: char **tokens - Array of tokens from the line
          int token_count - Number of tokens in the array
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          int *current_ic_ptr - Pointer to current instruction counter
          int line_num - Current line number for error reporting
Returns: int - TRUE if processing succeeded, FALSE on error
*/
static int process_line(char **tokens, int token_count, SymbolTable *symtab, MemoryImage *memory, int *current_ic_ptr, int line_num) {
    if (!check_line_format(tokens, token_count, line_num))
        return FALSE;

    if (is_directive_line(tokens, token_count)) {
        if (!process_directive_line(tokens, token_count, symtab, memory, line_num))
            return FALSE;
    } 
    else if (!process_instruction_line(tokens, token_count, symtab, memory, current_ic_ptr, line_num))
        return FALSE;

    return TRUE;
}

/*
Function to process all lines from .am file during second pass.
Manages memory for tokens and tracks errors across the entire file.
Receives: FILE *fp - Pointer to open source file
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
Returns: int - TRUE if all lines processed successfully, FALSE otherwise
*/
static int process_file_lines(FILE *fp, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    char **tokens = NULL;
    int token_count = 0, line_num = 0, error_flag = 0, second_pass_ic = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        if (!parse_tokens(line, &tokens, &token_count)) {
            print_line_error("Syntax error", NULL, line_num);
            error_flag = TRUE;
            continue;
        }
        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }
        if (!process_line(tokens, token_count, symtab, memory, &second_pass_ic, line_num)) {
            print_line_error("Detected issue while processing line", NULL, line_num);
            error_flag = TRUE;
        }
        free_tokens(tokens, token_count);
    }
    return (error_flag ? FALSE : TRUE);
}

/*
Function to write instruction memory contents to .ob file in base4 format
Receives: FILE *fp - Pointer to output file
          MemoryImage *memory - Pointer to memory image
*/
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

/*
Function to write data memory contents to .ob file in base4 format.
Receives: FILE *fp - Pointer to output file
          MemoryImage *memory - Pointer to memory image
*/
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

/*
Function to create & write the .ob file.
Writes header with IC/DC, alongside instruction / data words.
Receives: const char *filename - Name of output file
          MemoryImage *memory - Pointer to memory image
*/
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

/*
Function to create and write the .ent file.
Lists all symbols marked as entry with their addresses.
Receives: const char *filename - Name of output file
          SymbolTable *symtab - Pointer to symbol table
*/
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

/*
Function to create and write the .ext file.
Lists all external symbol references with their usage locations.
Receives: const char *filename - Name of output file
          MemoryImage *memory - Pointer to memory image
          SymbolTable *symtab - Pointer to symbol table
*/
static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    const char *symbol_name;
    char addr_str[ADDR_LENGTH];
    int i, ext_index;
    FILE *fp = open_output_file(filename);

    if (!fp)
        return;

    for (i = IC_START; i < IC_START + memory->ic; i++) {
        ext_index = memory->words[i].operand.ext_symbol_index;

        if ((memory->words[i].operand.are == ARE_EXTERNAL) &&
            (ext_index >= 0)) {
            symbol_name = symtab->symbols[ext_index].name;

            convert_to_base4_address(i, addr_str);
            write_file_line(fp, symbol_name, addr_str);
        }
    }
    safe_fclose(&fp);
}

/* Outer methods */
/* ==================================================================== */
/*
Function to perform the second pass (last stage) of assembler on an .am file.
Opens file, processes all lines, resolves symbols, and generates output files.
Receives: const char *filename - Name of source file
          SymbolTable *symtab - Pointer to symbol table
          MemoryImage *memory - Pointer to memory image
          const char *obj_file - Name for .ob file
          const char *ent_file - Name for .ent file
          const char *ext_file - Name for .ext file
Returns: int - TRUE if second pass completed successfully, PASS_ERROR otherwise
*/
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