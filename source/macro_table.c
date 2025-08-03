#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"
#include "instructions.h"
#include "macro_table.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int validate_macro_name_availability(MacroTable *table, const char *name) {
    int i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, name) == 0)
            return FALSE;
    }
    return TRUE;
}

static char *extract_macro_name_from_call(const char *line) {
    char temp[MAX_LINE_LENGTH];
    char *token, *colon_pos;

    strcpy(temp, line);
    trim_whitespace(temp);

    colon_pos = strchr(temp, LABEL_TERMINATOR);

    if (colon_pos) {
        token = colon_pos + 1;

        while (isspace((unsigned char) *token)) {
            token++;
        }
        token = strtok(token, " \t\n");
    } else {
        token = strtok(temp, " \t\n");
    }

    return token;
}

/* Outer regular methods */
/* ==================================================================== */
int is_valid_macro_name(const char *name) {
    size_t len, j;

    if (get_instruction(name) != NULL)
        return FALSE;

    if (name[0] == DIRECTIVE_CHAR)
        return FALSE;

    len = strlen(name);

    if (len == 0 || len > MAX_MACRO_NAME_LENGTH - 1)
        return FALSE;

    if (!isalpha(name[0]))
        return FALSE;

    for (j = 1; j < len; j++) {
        if (!isalnum(name[j]) && name[j] != UNDERSCORE_CHAR)
            return FALSE;
    }
    return TRUE;
}

int add_macro(MacroTable *table, const char *name) {
    if (table->count >= MAX_MACROS)
        return FALSE;

    if (!validate_macro_name_availability(table, name))
        return FALSE;

    strncpy(table->macros[table->count].name, name, MAX_MACRO_NAME_LENGTH);
    table->macros[table->count].line_count = 0;
    table->count++;

    return TRUE;
}

const Macro *find_macro(const MacroTable *table, const char *name) {
    int i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, name) == 0)
            return &table->macros[i];
    }
    return NULL;
}

int is_macro_definition(const char *line) {
    char temp[MAX_LINE_LENGTH];

    strcpy(temp, line);
    trim_whitespace(temp);

    return strncmp(temp, MACRO_START, strlen(MACRO_START)) == 0;
}

int is_macro_end(const char *line) {
    char temp[MAX_LINE_LENGTH];

    strcpy(temp, line);
    trim_whitespace(temp);

    return strcmp(temp, MACRO_END) == 0;
}

int is_macro_call(const char *line, const MacroTable *table) {
    char *macro_name = extract_macro_name_from_call(line);
    
    if (macro_name)
        return find_macro(table, macro_name) != NULL;

    return FALSE;
}

void add_line_to_macro_body(const char *original_line, Macro *current_macro) {
    char temp_body_line[MAX_LINE_LENGTH];

    strcpy(temp_body_line, original_line);
    remove_comments(temp_body_line);
    trim_whitespace(temp_body_line);

    if (strlen(temp_body_line) > 0) {
        sprintf(current_macro->body[current_macro->line_count], "%s\n", temp_body_line);
        current_macro->line_count++;
    }
}