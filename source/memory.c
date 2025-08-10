#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "errors.h"

void init_memory(MemoryImage *memory) {
    int i;
    memory->ic = 0;
    memory->dc = 0;

    for (i = 0; i < MAX_WORD_COUNT; i++) {
        memset(&memory->words[i], 0, sizeof(MemoryWord));
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

/* Store a 10-bit signed value in the data segment */
int store_value(MemoryImage *memory, int value) {
    if (!validate_dc_limit(memory->dc))
        return FALSE;

    /* Store the data word at the current data counter index */
    memory->words[memory->dc].data.value = value & WORD_MASK;  /* Enforce 10-bit */
    memory->dc++;

    return TRUE;
}