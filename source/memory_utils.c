#include "memory_layout.h"
#include "utils.h"

int has_entries(const SymbolTable *symtab) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].is_entry) 
            return TRUE;
    }
    return FALSE;
}

int has_externs(const SymbolTable *symtab) {
    int i;
    
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == EXTERNAL_SYMBOL) 
            return TRUE;
    }
    return FALSE;
}