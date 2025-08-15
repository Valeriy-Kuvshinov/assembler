#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "errors.h"

/*
Function to initialize a MemoryImage structure with default values, and clears all memory words
Receives: MemoryImage *memory - Pointer to memory structure to initialize
*/
void init_memory(MemoryImage *memory) {
    int i;

    memory->ic = 0;
    memory->dc = 0;

    for (i = 0; i < MAX_WORD_COUNT; i++) {
        memset(&memory->words[i], 0, sizeof(MemoryWord));
    }
}

/*
Function to validate instruction counter against maximum allowed value.
Receives: int current_ic - Current instruction counter value
Returns: int - TRUE if within limits, FALSE if overflow
*/
int check_ic_limit(int current_ic) {
    if (current_ic >= MAX_IC_SIZE) {
        print_error("IC memory overflow", "Exceeded instruction words limit");
        return FALSE;
    }
    return TRUE;
}

/*
Function to validate data counter against maximum allowed value.
Receives: int current_dc - Current data counter value
Returns: int - TRUE if within limits, FALSE if overflow
*/
int check_dc_limit(int current_dc) {
    if (current_dc >= MAX_DC_SIZE) {
        print_error("DC memory overflow", "Exceeded data words limit");
        return FALSE;
    }
    return TRUE;
}

/*
Function to store a 10-bit signed value in the data segment.
Automatically increments data counter (dc) and applies WORD_MASK to enforce 10-bit storage.
Receives: MemoryImage *memory - Target memory structure
          int value - Value to store (will be masked to 10 bits)
Returns: int - TRUE if stored successfully, FALSE if:
               - DC limit exceeded (via check_dc_limit)
               - Memory pointer is NULL
*/
int store_value(MemoryImage *memory, int value) {
    if (!check_dc_limit(memory->dc))
        return FALSE;

    memory->words[memory->dc].data.value = value & WORD_MASK;
    memory->dc++;

    return TRUE;
}