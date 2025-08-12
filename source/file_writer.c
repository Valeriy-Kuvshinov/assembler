#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "symbol_table.h"
#include "encoder.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
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

/* Outer regular methods */
/* ==================================================================== */
void write_object_file(const char *filename, MemoryImage *memory) {
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

void write_entry_file(const char *filename, SymbolTable *symbol_table) {
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

void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symbol_table) {
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