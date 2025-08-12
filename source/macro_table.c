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
static int resize_macro_body(Macro *macro) {
    int new_capacity;
    char **new_body;

    if (!macro) 
        return FALSE;

    new_capacity = (macro->body_capacity == 0) ? INITIAL_MACRO_BODY_CAPACITY : macro->body_capacity * 2;
    new_body = realloc(macro->body, new_capacity * sizeof(char*));

    if (!new_body) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize macro body");
        return FALSE;
    }

    macro->body = new_body;
    macro->body_capacity = new_capacity;

    return TRUE;
}

static int resize_macro_table(MacroTable *table) {
    int new_capacity;
    Macro *new_macros;

    new_capacity = (table->capacity == 0) ? INITIAL_MACROS_CAPACITY : table->capacity * 2;
    new_macros = realloc(table->macros, new_capacity * sizeof(Macro));

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
    size_t length;

    if (!macro || !*macro) {
        print_error(ERR_MACRO_SYNTAX, NULL);
        return FALSE;
    }

    length = strlen(macro);

    if (length >= MAX_MACRO_NAME_LENGTH) {
        print_error(ERR_MACRO_SYNTAX, macro);
        return FALSE;
    }

    if (!isalpha(macro[0])) {
        print_error(ERR_MACRO_SYNTAX, macro);
        return FALSE;
    }

    for (i = 1; i < length; i++) {
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

/* Initializes a single Macro to a known empty state */
int init_macro(Macro *macro, const char *name) {
    if (!macro) {
        print_error(ERR_MEMORY_ALLOCATION, "for macro initialization");
        return FALSE;
    }

    if (name) {
        strncpy(macro->name, name, MAX_MACRO_NAME_LENGTH - 1);
        macro->name[MAX_MACRO_NAME_LENGTH - 1] = NULL_TERMINATOR;
    } else
        macro->name[0] = NULL_TERMINATOR;

    macro->body = NULL;
    macro->line_count = 0;
    macro->body_capacity = INITIAL_MACRO_BODY_CAPACITY;

    return TRUE;
}

static int store_macro(MacroTable *table, const char *name) {
    if (!init_macro(&table->macros[table->count], name))
        return FALSE;

    table->count++;

    return TRUE;
}

/* Frees a single macro's body and resets its fields */
void free_macro(Macro *macro) {
    int i;

    if (!macro)
        return;

    if (macro->body) {
        for (i = 0; i < macro->line_count; i++) {
            safe_free((void**)&macro->body[i]); /* free each line */
        }
        safe_free((void**)&macro->body); /* free array of char* */
    }
    macro->line_count = 0;
    macro->body_capacity = 0;
    macro->name[0] = NULL_TERMINATOR;
}

/* Outer regular methods */
/* ==================================================================== */
int init_macro_table(MacroTable *table) {
    table->macros = malloc(INITIAL_MACROS_CAPACITY * sizeof(Macro));

    if (!table->macros) {
        print_error(ERR_MEMORY_ALLOCATION, "for macro table initialization");
        return FALSE;
    }

    table->count = 0;
    table->capacity = INITIAL_MACROS_CAPACITY;

    return TRUE;
}

void free_macro_table(MacroTable *table) {
    int i;

    if (!table || !table->macros)
        return;

    for (i = 0; i < table->count; i++) {
        free_macro(&table->macros[i]);
    }

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

    if (!store_macro(table, name)) {
        print_error("Failed to store macro", name);
        return FALSE;
    }
    return TRUE;
}

int add_line_to_macro(Macro *macro, const char *line_content) {
    char *line_copy;
    size_t length;

    if (!macro || !line_content || line_content[0] == NULL_TERMINATOR)
        return FALSE;

    if (macro->body == NULL) {
        macro->body = malloc(INITIAL_MACRO_BODY_CAPACITY * sizeof(char*));

        if (!macro->body) {
            print_error(ERR_MEMORY_ALLOCATION, "Failed to allocate macro body");
            return FALSE;
        }

        macro->body_capacity = INITIAL_MACRO_BODY_CAPACITY;
    } else if (macro->line_count >= macro->body_capacity) {
        if (!resize_macro_body(macro))
            return FALSE;
    }

    length = strlen(line_content);
    line_copy = malloc(length + 1);

    if (!line_copy) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to allocate memory for macro line");
        return FALSE;
    }

    strcpy(line_copy, line_content);
    macro->body[macro->line_count++] = line_copy;

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