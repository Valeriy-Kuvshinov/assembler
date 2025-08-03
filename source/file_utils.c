#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "base4.h"
#include "memory.h"
#include "instructions.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void write_object_file(const char *filename, MemoryImage *memory) {
    char ic_str[10], dc_str[10];
    char addr_str[ADDR_LENGTH], value_str[WORD_LENGTH];
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create object file", filename);
        return;
    }
    
    /* Write header */
    printf("DEBUG: Writing header - IC = %d, DC = %d\n", memory->ic, memory->dc);
    convert_to_base4_header(memory->ic, ic_str);
    convert_to_base4_header(memory->dc, dc_str);
    printf("DEBUG: Converted to base4 - IC = %s, DC = %s\n", ic_str, dc_str);
    
    fprintf(fp, "%s %s\n", ic_str, dc_str);
    
    /* Write instructions */
    for (i = INSTRUCTION_START; i < INSTRUCTION_START + memory->ic; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    
    /* Write data */
    for (i = INSTRUCTION_START + memory->ic; i < INSTRUCTION_START + memory->ic + memory->dc; i++) {
        convert_to_base4_address(i, addr_str);
        convert_to_base4_word(memory->words[i].value, value_str);
        fprintf(fp, "%s %s\n", addr_str, value_str);
    }
    fclose(fp);
}

static void write_entry_file(const char *filename, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int i;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create entry file", filename);
        return;
    }
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) {
            convert_to_base4_address(symtab->symbols[i].value, addr_str);
            fprintf(fp, "%s %s\n", symtab->symbols[i].name, addr_str);
        }
    }
    fclose(fp);
}

static void write_extern_file(const char *filename, MemoryImage *memory, SymbolTable *symtab) {
    char addr_str[ADDR_LENGTH];
    int j;
    FILE *fp = fopen(filename, "w");
    
    if (!fp) {
        print_error("Failed to create extern file", filename);
        return;
    }
    
    for (j = INSTRUCTION_START; j < INSTRUCTION_START + memory->ic; j++) {
        if (memory->words[j].are == ARE_EXTERNAL && memory->words[j].ext_symbol_index >= 0) {
            convert_to_base4_address(j, addr_str);
            fprintf(fp, "%s %s\n", 
                    symtab->symbols[memory->words[j].ext_symbol_index].name, 
                    addr_str);
        }
    }
    fclose(fp);
}

/* Outer regular methods */
/* ==================================================================== */
FILE *open_source_file(const char *filename) {
    FILE *src = fopen(filename, "r");
    
    if (!src)
        perror("Error opening source file");
    
    return src;
}

FILE *open_output_file(const char *filename) {
    FILE *am = fopen(filename, "w");
    
    if (!am)
        perror("Error creating output file");
    
    return am;
}

void write_output_files(MemoryImage *memory, SymbolTable *symtab, const char *obj_file, const char *ent_file, const char *ext_file) {
    int i, external_ref_count = 0;
    
    write_object_file(obj_file, memory);

    if (has_entries(symtab))
        write_entry_file(ent_file, symtab);

    if (has_externs(symtab)) {
        /* Count external references */
        for (i = INSTRUCTION_START; i < INSTRUCTION_START + memory->ic; i++) {
            if (memory->words[i].are == ARE_EXTERNAL) {
                printf("DEBUG: Found external reference at address %d\n", i);
                external_ref_count++;
            }
        }
        
        printf("DEBUG: Total external references found: %d\n", external_ref_count);
        
        if (external_ref_count > 0)
            write_extern_file(ext_file, memory, symtab);
    }
}