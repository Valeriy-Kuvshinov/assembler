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
/*
Function to resize the body of a macro when more space is needed.
Receives: Macro *macro - Pointer to the macro whose body needs resizing
Returns: int - TRUE if resizing succeeded, FALSE otherwise
*/
static int resize_macro_body(Macro *macro) {
    char **new_body;
    int new_capacity;

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

/*
Function to resize the macro table when more space is needed.
Receives: MacroTable *table - Pointer to the macro table to resize
Returns: int - TRUE if resizing succeeded, FALSE otherwise
*/
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

/*
Function to validate macro syntax.
Checks:
- Non-empty string
- Length within limits
- Starts with letter
- Contains only alphanumeric chars or '_'
Receives: const char *macro - The macro name to validate
Returns: int - TRUE if syntax is valid, FALSE otherwise
*/
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
        if (!isalnum(macro[i]) && (macro[i] != UNDERSCORE_CHAR)) {
            print_error(ERR_MACRO_SYNTAX, macro);
            return FALSE;
        }
    }
    return TRUE;
}

/*
Function to check if macro matches any reserved keywords.
Checks:
- Register names (r0-r7, PSW)
- Instruction names
- Directive names
- Macro keywords
Receives: const char *macro - The macro name to check
Returns: int - TRUE if name is a reserved word, FALSE otherwise
*/
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

/*
Function to initialize a macro with default values and sets its name.
Receives: Macro *macro - Pointer to the Macro structure to initialize
          const char *name - Name to assign to the macro
Returns: int - TRUE if initialization succeeded, FALSE otherwise
*/
static int init_macro(Macro *macro, const char *name) {
    if (!macro) {
        print_error(ERR_MEMORY_ALLOCATION, "for macro initialization");
        return FALSE;
    }
    strncpy(macro->name, name, MAX_MACRO_NAME_LENGTH - 1);
    macro->name[MAX_MACRO_NAME_LENGTH - 1] = NULL_TERMINATOR;

    macro->body = NULL;
    macro->line_count = 0;
    macro->body_capacity = INITIAL_MACRO_BODY_CAPACITY;

    return TRUE;
}

/*
Function to store a new macro in the macro table.
Receives: MacroTable *table - Pointer to the macro table
          const char *name - Name of the macro to store
Returns: int - TRUE if storage succeeded, FALSE otherwise
*/
static int store_macro(MacroTable *table, const char *name) {
    if (!init_macro(&table->macros[table->count], name))
        return FALSE;

    table->count++;
    return TRUE;
}

/*
Function to free all memory allocated for a macro and resets it.
Receives: Macro *macro - Pointer to the macro to free
*/
void free_macro(Macro *macro) {
    int i;

    if (!macro)
        return;

    if (macro->body) {
        for (i = 0; i < macro->line_count; i++) {
            safe_free((void**)&macro->body[i]);
        }
        safe_free((void**)&macro->body);
    }
    macro->line_count = 0;
    macro->body_capacity = 0;
    macro->name[0] = NULL_TERMINATOR;
}

/* Outer methods */
/* ==================================================================== */
/*
Function to initialize an empty macro table with default capacity.
Receives: MacroTable *table - Pointer to the table to initialize
Returns: int - TRUE if initialization succeeded, FALSE otherwise
*/
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

/*
Function to free all memory allocated for the macro table.
Receives: MacroTable *table - Pointer to the table to free
*/
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

/*
Function to validate if a string would make a proper macro name start.
Receives: const char *macro - The potential macro name to validate
Returns: int - TRUE if valid macro name, FALSE otherwise
*/
int is_valid_macro_start(const char *macro) {
    if (!is_valid_syntax(macro))
        return FALSE;

    if (is_reserved_word(macro))
        return FALSE;

    return TRUE;
}

/*
Function to find a macro in the table by name.
Receives: const MacroTable *table - Pointer to the table to search
          const char *macro - Name of the macro to find
Returns: const Macro* - Pointer to found macro or NULL if not found
*/
const Macro *find_macro(const MacroTable *table, const char *macro) {
    int i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, macro) == 0)
            return &table->macros[i];
    }
    return NULL;
}

/*
Function to add a new macro to the table after validation.
Receives: MacroTable *table - Pointer to the macro table
          const char *name - Name of the macro to add
Returns: int - TRUE if addition succeeded, FALSE otherwise
*/
int add_macro(MacroTable *table, const char *name) {
    if (!table) {
        print_error("Macro table not initialized", NULL);
        return FALSE;
    }
    if (find_macro(table, name)) {
        print_error("Duplicate macro name", name);
        return FALSE;
    }
    if ((table->count >= table->capacity) &&
        (!resize_macro_table(table)))
        return FALSE;

    if (!store_macro(table, name)) {
        print_error("Failed to store macro", name);
        return FALSE;
    }
    return TRUE;
}

/*
Function to add a string line to a macro, resizing body if needed.
Receives: Macro *macro - Pointer to the macro to add to
          const char *line_content - The line content to add
Returns: int - TRUE if addition succeeded, FALSE otherwise
*/
int add_line_to_macro(Macro *macro, const char *line_content) {
    if (!macro || !line_content || (line_content[0] == NULL_TERMINATOR))
        return FALSE;

    if (!macro->body) {
        macro->body = malloc(INITIAL_MACRO_BODY_CAPACITY * sizeof(char*));

        if (!macro->body) {
            print_error(ERR_MEMORY_ALLOCATION, "Failed to allocate macro body");
            return FALSE;
        }
        macro->body_capacity = INITIAL_MACRO_BODY_CAPACITY;
    }
    else if ((macro->line_count >= macro->body_capacity) &&
             (!resize_macro_body(macro)))
        return FALSE;

    macro->body[macro->line_count] = copy_string(line_content);

    if (!macro->body[macro->line_count]) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to allocate memory for macro line");
        return FALSE;
    }
    macro->line_count++;
    return TRUE;
}

/*
Function to check if a line contains a valid macro call.
Receives: const char *line - The line to check
          const MacroTable *table - Pointer to the macro table
Returns: int - TRUE if line contains a macro call, FALSE otherwise
*/
int is_macro_call(const char *line, const MacroTable *table) {
    char trimmed[MAX_LINE_LENGTH];
    char *colon_pos, *macro_part;

    strcpy(trimmed, line);
    trim_whitespace(trimmed);

    colon_pos = strchr(trimmed, LABEL_TERMINATOR);

    if (colon_pos) {
        if (!isspace(*(colon_pos + 1)))
            return FALSE;

        macro_part = colon_pos + 1;

        while (isspace(*macro_part)) {
            macro_part++;
        }
        return (find_macro(table, macro_part) != NULL);
    }
    return (find_macro(table, trimmed) != NULL);
}