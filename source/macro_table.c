#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "memory.h"
#include "instructions.h"
#include "directives.h"
#include "macro_table.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int resize_macro_table(MacroTable *table) {
    int new_capacity = (table->capacity == 0) ? INITIAL_MACROS_CAPACITY : table->capacity * 2;
    Macro *new_macros = realloc(table->macros, new_capacity * sizeof(Macro));

    if (!new_macros) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize macro table");
        return FALSE;
    }
    table->macros = new_macros;
    table->capacity = new_capacity;

    return TRUE;
}

static int is_valid_syntax(const char *macro) {
    int i;
    
    if (!macro || !*macro) {
        print_error(ERR_MACRO_SYNTAX, NULL);
        return FALSE;
    }
    
    if (strlen(macro) >= MAX_MACRO_NAME_LENGTH) {
        print_error(ERR_MACRO_SYNTAX, macro);
        return FALSE;
    }
    
    if (!isalpha(macro[0])) {
        print_error(ERR_MACRO_SYNTAX, macro);
        return FALSE;
    }
    
    for (i = 1; macro[i]; i++) {
        if (!isalnum(macro[i]) && macro[i] != UNDERSCORE_CHAR) {
            print_error(ERR_MACRO_SYNTAX, macro);
            return FALSE;
        }
    }
    return TRUE;
}

static int is_reserved_word(const char *macro) {
    if (IS_REGISTER_OR_PSW(macro)) {
        print_error("Macro name cannot be register (r0 - r7 / PSW)", macro);
        return TRUE;
    }

    if (IS_INSTRUCTION(macro)) {
        print_error("Macro name cannot be instruction (mov / cmp / add / ...)", macro);
        return TRUE;
    }

    if (IS_DIRECTIVE(macro)) {
        print_error("Macro name cannot be directive (.data / .string / .mat / .entry / .extern)", macro);
        return TRUE;
    }

    if (IS_MACRO_KEYWORD(macro)) {
        print_error("Macro name cannot be macro keyword (mcro / mcroend)", macro);
        return TRUE;
    }
    return FALSE;
}

static void store_macro(MacroTable *table, const char *name) {
    strncpy(table->macros[table->count].name, name, MAX_MACRO_NAME_LENGTH - 1);

    table->macros[table->count].name[MAX_MACRO_NAME_LENGTH - 1] = NULL_TERMINATOR;
    table->macros[table->count].line_count = 0;
    table->count++;
}

/* Outer regular methods */
/* ==================================================================== */
int init_macro_table(MacroTable *table) {
    table->macros = malloc(INITIAL_MACROS_CAPACITY * sizeof(Macro));

    if (!table->macros) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to initialize macro table");
        return FALSE;
    }
    table->count = 0;
    table->capacity = INITIAL_MACROS_CAPACITY;

    return TRUE;
}

void free_macro_table(MacroTable *table) {
    safe_free((void**)&table->macros);

    table->count = 0;
    table->capacity = 0;
}

int is_valid_macro_start(const char *macro) {
    if (!is_valid_syntax(macro))
        return FALSE;
    
    if (is_reserved_word(macro))
        return FALSE;
    
    return TRUE;
}

const Macro *find_macro(const MacroTable *table, const char *macro) {
    int i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, macro) == 0)
            return &table->macros[i];
    }
    return NULL;
}

int add_macro(MacroTable *table, const char *name) {
    if (!table) {
        print_error("Macro table not initialized", NULL);
        return FALSE;
    }

    if (find_macro(table, name) != NULL) {
        print_error("Duplicate macro name", name);
        return FALSE;
    }

    if (table->count >= table->capacity) {
        if (!resize_macro_table(table))
            return FALSE;
    }
    store_macro(table, name);

    return TRUE;
}

int is_macro_call(const char *line, const MacroTable *table) {
    char trimmed[MAX_LINE_LENGTH];
    char *colon_pos;
    
    strcpy(trimmed, line);
    trim_whitespace(trimmed);
    
    /* Handle labels: "LABEL: macro_name" */
    colon_pos = strchr(trimmed, LABEL_TERMINATOR);

    if (colon_pos) {
        char *macro_part = colon_pos + 1;

        while (isspace(*macro_part)) {
            macro_part++;
        }
        return find_macro(table, macro_part) != NULL;
    }
    return find_macro(table, trimmed) != NULL;
}