#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"
#include "instructions.h"
#include "macro_table.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static void init_macro_table(MacroTable *table) {
    int i;

    table->count = 0;

    for (i = 0; i < MAX_MACROS; i++) {
        table->macros[i].line_count = 0;
        table->macros[i].name[0] = NULL_TERMINATOR;
    }
}

static int validate_macro_definition_syntax(char *line, char **name, int line_number) {
    char context[100];
    char *extra;

    *name = strtok(line + strlen(MACRO_START), " \t");

    if (!*name) {
        sprintf(context, "line %d", line_number);
        print_error(ERR_MISSING_MACRO_NAME, context);
        return FALSE;
    }

    extra = strtok(NULL, " \t\n");

    if (extra) {
        sprintf(context, "'%s' after macro name '%s' at line %d", extra, *name, line_number);
        print_error(ERR_UNEXPECTED_TOKEN, context);
        return FALSE;
    }
    return TRUE;
}

static int validate_and_add_macro(MacroTable *macro_table, const char *name, int line_number) {
    char context[100];

    if (!is_valid_macro_name(name)) {
        sprintf(context, "'%s' at line %d", name, line_number);
        print_error(ERR_INVALID_MACRO_NAME, context);
        return FALSE;
    }

    if (!add_macro(macro_table, name)) {
        sprintf(context, "'%s' at line %d", name, line_number);
        print_error(ERR_DUPLICATE_MACRO, context);
        return FALSE;
    }
    return TRUE;
}

static int process_macro_definition(char *line, MacroTable *macro_table, Macro **current_macro, int *in_macro_definition, int line_number) {
    char *name;

    if (!validate_macro_definition_syntax(line, &name, line_number))
        return FALSE;

    if (!validate_and_add_macro(macro_table, name, line_number))
        return FALSE;

    *in_macro_definition = 1;
    *current_macro = &macro_table->macros[macro_table->count - 1];

    return TRUE;
}

static int validate_macro_end_syntax(char *line, int line_number) {
    char *extra = strtok(line + strlen(MACRO_END), " \t\n");

    if (extra) {
        char context[100];

        sprintf(context, "'%s' after 'endmcro' at line %d", extra, line_number);
        print_error(ERR_UNEXPECTED_TOKEN, context);

        return FALSE;
    }
    return TRUE;
}

static void process_macro_body(const char *original_line, Macro *current_macro, int line_number) {
    if (current_macro->line_count < MAX_MACRO_BODY)
        add_line_to_macro_body(original_line, current_macro);
    else {
        char context[100];

        sprintf(context, "macro '%s', line %d", current_macro->name, line_number);
        print_error(ERR_MACRO_OVERFLOW, context);
    }
}

static int process_macro_definition_line(char *line, MacroTable *macro_table, Macro **current_macro, int *in_macro_definition, int line_number) {
    if (is_macro_definition(line))
        return process_macro_definition(line, macro_table, current_macro, in_macro_definition, line_number);
    
    return TRUE;
}

static int process_macro_end_line(char *line, int *in_macro_definition, Macro **current_macro, int line_number) {
    if (is_macro_end(line)) {
        if (!validate_macro_end_syntax(line, line_number))
            return FALSE;
        
        *in_macro_definition = 0;
        *current_macro = NULL;

        return TRUE;
    }
    return FALSE; /* Not a macro end line */
}

static int process_single_line_collection(char *line, char *original_line, MacroTable *macro_table, 
                                        Macro **current_macro, int *in_macro_definition, int line_number) {
    remove_comments(line);
    trim_whitespace(line);

    if (*in_macro_definition) {
        if (process_macro_end_line(line, in_macro_definition, current_macro, line_number))
            return TRUE;
        
        if (*current_macro)
            process_macro_body(original_line, *current_macro, line_number);
    } else {
        if (!process_macro_definition_line(line, macro_table, current_macro, in_macro_definition, line_number))
            return FALSE;
    }
    return TRUE;
}

static int collect_macro_definitions(FILE *src, MacroTable *macro_table) {
    char line[MAX_LINE_LENGTH], original_line[MAX_LINE_LENGTH];
    int in_macro_definition = 0, line_number = 0, error = 0;
    Macro *current_macro = NULL;

    while (fgets(line, MAX_LINE_LENGTH, src)) {
        line_number++;
        strcpy(original_line, line);

        if (!process_single_line_collection(line, original_line, macro_table, &current_macro, &in_macro_definition, line_number))
            error = TRUE;
    }
    return (error == 0) ? TRUE : FALSE;
}

static void write_label_if_present(FILE *am, char *temp_line) {
    char *colon_pos = strchr(temp_line, LABEL_TERMINATOR);
    
    if (colon_pos) {
        *colon_pos = NULL_TERMINATOR;
        fprintf(am, "%s:\n", temp_line);
    }
}

static char *extract_macro_name_for_expansion(char *temp_line) {
    char *colon_pos = strchr(temp_line, LABEL_TERMINATOR);
    char *macro_name;

    if (colon_pos) {
        macro_name = colon_pos + 1;

        while (isspace((unsigned char)*macro_name)) {
            macro_name++;
        }
        macro_name = strtok(macro_name, " \t\n");
    } else
        macro_name = strtok(temp_line, " \t\n");

    return macro_name;
}

static void write_macro_body(FILE *am, const Macro *macro) {
    int i;
    
    for (i = 0; i < macro->line_count; i++) {
        fputs(macro->body[i], am);
    }
}

static void expand_macro_call(FILE *am, const char *processed_line, const MacroTable *macro_table) {
    char temp_line[MAX_LINE_LENGTH];
    char *macro_name;

    strcpy(temp_line, processed_line);
    
    write_label_if_present(am, temp_line);
    macro_name = extract_macro_name_for_expansion(temp_line);

    if (macro_name) {
        const Macro *macro = find_macro(macro_table, macro_name);

        if (macro)
            write_macro_body(am, macro);
    }
}

static int process_single_line_expansion(char *line, char *processed_line, FILE *am, const MacroTable *macro_table, int *in_macro_definition) {
    strcpy(processed_line, line);
    remove_comments(processed_line);
    trim_whitespace(processed_line);

    if (*in_macro_definition) {
        if (is_macro_end(processed_line))
            *in_macro_definition = 0;

        return TRUE; /* Skip macro definition lines */
    }

    if (is_macro_definition(processed_line)) {
        *in_macro_definition = 1;
        return TRUE; /* Skip macro definition lines */
    }

    if (is_macro_call(processed_line, macro_table)) {
        expand_macro_call(am, processed_line, macro_table);
        return TRUE;
    }

    if (!should_skip_line(processed_line))
        fprintf(am, "%s\n", processed_line);
    
    return TRUE;
}

static int expand_macros(FILE *src, FILE *am, const MacroTable *macro_table) {
    char line[MAX_LINE_LENGTH], processed_line[MAX_LINE_LENGTH];
    int in_macro_definition = 0;

    while (fgets(line, MAX_LINE_LENGTH, src)) {
        if (!process_single_line_expansion(line, processed_line, am, macro_table, &in_macro_definition))
            return FALSE;
    }
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int preprocess_macros(const char *src_filename, const char *am_filename) {
    FILE *src, *am;
    MacroTable macro_table;

    init_macro_table(&macro_table);

    src = open_source_file(src_filename);

    if (!src)
        return FALSE;

    /* First pass: collect macro definitions */
    if (!collect_macro_definitions(src, &macro_table)) {
        safe_fclose(&src);
        return FALSE;
    }

    rewind(src);

    am = open_output_file(am_filename);

    if (!am) {
        safe_fclose(&src);
        return FALSE;
    }

    /* Second pass: expand macros */
    if (!expand_macros(src, am, &macro_table)) {
        safe_fclose(&src);
        safe_fclose(&am);
        return FALSE;
    }

    safe_fclose(&src);
    safe_fclose(&am);

    return TRUE;
}