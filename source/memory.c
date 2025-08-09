#include "memory.h"
#include "instructions.h"

void init_memory(MemoryImage *memory) {
    int i;

    memory->ic = 0;
    memory->dc = 0;
    
    for (i = 0; i < MAX_WORD_COUNT; i++) {
        memory->words[i].value = 0;
        memory->words[i].are = ARE_ABSOLUTE;
        memory->words[i].ext_symbol_index = -1;
    }
}

int validate_ic_limit(int current_ic) {
    if (current_ic >= MAX_IC_SIZE) {
        print_error("Exceeded instruction words limit", NULL);
        return FALSE;
    }
    return TRUE;
}

int validate_dc_limit(int current_dc) {
    if (current_dc >= MAX_DC_SIZE) {
        print_error("Exceeded data words limit", NULL);
        return FALSE;
    }
    return TRUE;
}

int store_value(MemoryImage *memory, int value) {
    if (!validate_dc_limit(memory->dc))
        return FALSE;
    
    memory->words[INSTRUCTION_START + memory->ic + memory->dc].value = value & WORD_MASK;
    memory->words[INSTRUCTION_START + memory->ic + memory->dc].are = ARE_ABSOLUTE;
    memory->dc++;

    return TRUE;
}