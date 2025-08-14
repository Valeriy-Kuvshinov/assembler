#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "macro_table.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int is_empty_line(const char *line) {
    if (!line) 
        return TRUE;
    
    while (*line) {
        if (!isspace(*line))
            return FALSE;

        line++;
    }
    return TRUE;
}

static char *extract_macro_name(const char *line) {
    static char temp_buffer[MAX_LINE_LENGTH];
    char *macro_name_start;
    char *colon_pos;

    if (!bounded_string_copy(temp_buffer, line, sizeof(temp_buffer), "extract macro name"))
        return NULL;

    colon_pos = strchr(temp_buffer, LABEL_TERMINATOR);

    if (colon_pos) {
        /* Handle both cases: "LABEL: macro" and "LABEL:macro" */
        macro_name_start = colon_pos + 1;

        while (isspace(*macro_name_start)) {
            macro_name_start++;
        }

        if (*macro_name_start == NULL_TERMINATOR)
            return NULL;

        memmove(temp_buffer, macro_name_start, strlen(macro_name_start) + 1);
        trim_whitespace(temp_buffer);

        if (is_empty_line(temp_buffer))
            return NULL;

        return temp_buffer;
    } else {
        /* No label, the whole line is the macro name. */
        trim_whitespace(temp_buffer);

        if (is_empty_line(temp_buffer))
            return NULL;

        return temp_buffer;
    }
}

static int parse_macro_declaration(char *line, char **name, int line_num) {
    size_t length;

    length = strlen(MACRO_START);
    *name = strtok(line + length, SPACE_TAB);

    if (!*name) {
        print_line_error("Missing name after macro declaration", NULL, line_num);
        return FALSE;
    }
    return TRUE;
}

static int check_macro_name(const char *name, int line_num) {
    if (!is_valid_macro_start(name)) {
        print_line_error("Invalid macro name", name, line_num);
        return FALSE;
    }
    return TRUE;
}

static int check_macro_duplicate(const MacroTable *table, const char *name, int line_num) {
    if (find_macro(table, name) != NULL) {
        print_line_error("Duplicate macro name", name, line_num);
        return FALSE;
    }
    return TRUE;
}

static int create_new_macro(MacroTable *table, const char *name, int line_num, Macro **out_macro) {
    if (!check_macro_name(name, line_num))
        return FALSE;

    if (!check_macro_duplicate(table, name, line_num))
        return FALSE;

    if (!add_macro(table, name)) {
        print_line_error("Failed to create macro", name, line_num);
        return FALSE;
    }
    *out_macro = &table->macros[table->count - 1];
    return TRUE;
}

static int process_macro_definition(char *line, MacroTable *macrotab, Macro **current_macro, int line_num) {
    char *name;

    if (!parse_macro_declaration(line, &name, line_num))
        return FALSE;

    return create_new_macro(macrotab, name, line_num, current_macro);
}

static void process_macro_body(const char *original_line, Macro *current_macro, int line_num) {
    char clean_line[MAX_LINE_LENGTH];
    size_t length;

    bounded_string_copy(clean_line, original_line, sizeof(clean_line), "macro body processing");
    preprocess_line(clean_line);

    length = strlen(clean_line);

    if (length > MAX_LINE_LENGTH - 1) {
        clean_line[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
        print_line_warning("Line truncated in macro", current_macro->name, line_num);
    }

    if (!add_line_to_macro(current_macro, clean_line))
        print_line_error("Failed to add line to macro", current_macro->name, line_num);
}

static void get_indentation(const char *line, char *indent) {
    int i = 0;

    while (line[i] && (line[i] == ' ' || line[i] == '\t')) {
        indent[i] = line[i];
        i++;
    }
    indent[i] = NULL_TERMINATOR;
}

static void write_macro_body(FILE *am, const Macro *macro, const char *indent) {
    int i;

    for (i = 0; i < macro->line_count; i++) {
        if (macro->body[i] && macro->body[i][0] != NULL_TERMINATOR)
            write_file_line(am, indent, macro->body[i]);
    }
}

static int process_macro_call_line(const char *line, FILE *am, const MacroTable *macrotab, int line_num) {
    char *macro_name;
    char indent[MAX_LINE_LENGTH] = "";
    const Macro *macro;

    macro_name = extract_macro_name(line);

    if (!macro_name) {
        print_line_error("Empty macro name after label", NULL, line_num);
        return FALSE;
    }

    macro = find_macro(macrotab, macro_name);

    if (!macro) {
        print_line_error("Undefined macro", macro_name, line_num);
        return FALSE;
    }
    get_indentation(line, indent);
    write_macro_body(am, macro, indent);

    return TRUE;
}

static int process_line(const char *original_line, const char *processed_line, FILE *am_fp, MacroTable *macrotab, Macro **current_macro, int *in_macro_definition, int line_num) {
    char temp_line[MAX_LINE_LENGTH];

    if (is_empty_line(processed_line))
        return TRUE; 

    /* process lines within a macro definition */
    if (*in_macro_definition) {
        if (IS_MACRO_END(processed_line)) {
            *in_macro_definition = FALSE;
            *current_macro = NULL;
        } else
            process_macro_body(original_line, *current_macro, line_num);

        return TRUE;
    }

    /* process lines outside a macro definition */
    if (IS_MACRO_DEFINITION(processed_line)) {
        bounded_string_copy(temp_line, processed_line, sizeof(temp_line), "macro definition");
        *in_macro_definition = process_macro_definition(temp_line, macrotab, current_macro, line_num);
        return *in_macro_definition;
    }

    if (is_macro_call(processed_line, macrotab))
        return process_macro_call_line(original_line, am_fp, macrotab, line_num);
    
    fprintf(am_fp, "%s", original_line);
    return TRUE;
}

/* Outer methods */
/* ==================================================================== */
int preprocess_macros(const char *filename, const char *am_filename, MacroTable *macrotab) {
    FILE *src_fp = NULL, *am_fp = NULL;
    char line[MAX_LINE_LENGTH], original_line[MAX_LINE_LENGTH], processed_line[MAX_LINE_LENGTH];
    int line_num = 0;
    int in_macro_definition = FALSE, has_error = FALSE;
    Macro *current_macro = NULL;

    src_fp = open_source_file(filename);

    if (!src_fp)
        return PASS_ERROR;

    am_fp = open_output_file(am_filename);

    if (!am_fp) {
        safe_fclose(&src_fp);
        return PASS_ERROR;
    }

    while (fgets(line, MAX_LINE_LENGTH, src_fp)) {
        line_num++;

        bounded_string_copy(original_line, line, sizeof(original_line), "line processing");
        bounded_string_copy(processed_line, line, sizeof(processed_line), "line processing");
        preprocess_line(processed_line);

        if (!process_line(original_line, processed_line, am_fp, macrotab, &current_macro, &in_macro_definition, line_num))
            has_error = TRUE;
    }

    if (in_macro_definition) {
        print_error("Macro not closed with 'mcroend'", NULL);
        has_error = TRUE;
    }
    safe_fclose(&src_fp);
    safe_fclose(&am_fp);

    return (has_error ? PASS_ERROR : TRUE);
}