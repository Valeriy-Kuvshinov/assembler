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
static char *extract_macro_name(const char *line) {
    static char temp_buffer[MAX_LINE_LENGTH];
    char *macro_name_start;
    char *colon_pos;

    strcpy(temp_buffer, line);
    colon_pos = strchr(temp_buffer, LABEL_TERMINATOR);

    if (colon_pos) {
        /* Handle both cases: "LABEL: macro" and "LABEL:macro" */
        macro_name_start = colon_pos + 1;
    
        while (isspace(*macro_name_start)) {
            macro_name_start++;
        }

        if (*macro_name_start == NULL_TERMINATOR)
            return NULL;

        strcpy(temp_buffer, macro_name_start);
        trim_whitespace(temp_buffer);
        
        if (*temp_buffer == NULL_TERMINATOR)
            return NULL;

        return temp_buffer;
    } else {
        /* No label, the whole line is the macro name. */
        trim_whitespace(temp_buffer);

        if (*temp_buffer == NULL_TERMINATOR)
            return NULL;

        return temp_buffer;
    }
}

static int parse_macro_declaration(char *line, char **name, int line_num) {
    size_t length;

    length = strlen(MACRO_START);
    *name = strtok(line + length, SPACE_TAB);

    if (!*name) {
        fprintf(stderr, "Missing name after '%s' declaration, line %d%c", MACRO_START, line_num, NEWLINE);
        return FALSE;
    }
    return TRUE;
}

static int process_macro_definition(char *line, MacroTable *macro_table, Macro **current_macro, int *in_macro_definition, int line_num) {
    char *name;

    if (!parse_macro_declaration(line, &name, line_num))
        return FALSE;

    if (!is_valid_macro_start(name)) {
        fprintf(stderr, "Invalid macro name '%s', line %d%c", name, line_num, NEWLINE);
        return FALSE;
    }

    if (!add_macro(macro_table, name)) {
        fprintf(stderr, "Duplicate macro, line %d%c", line_num, NEWLINE);
        return FALSE;
    }

    *in_macro_definition = 1;
    *current_macro = &macro_table->macros[macro_table->count - 1];

    return TRUE;
}

static void process_macro_body(const char *original_line, Macro *current_macro, int line_num) {
    char clean_line[MAX_LINE_LENGTH];
    size_t length;

    strcpy(clean_line, original_line);
    preprocess_line(clean_line);

    length = strlen(clean_line);

    if (length > MAX_LINE_LENGTH - 1) {
        clean_line[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
        length = MAX_LINE_LENGTH - 1;

        fprintf(stderr, "Warning: Line truncated to %d chars in macro '%s', line %d%c", 
                MAX_LINE_LENGTH - 1, current_macro->name, line_num, NEWLINE);
    }

    if (!add_line_to_macro(current_macro, clean_line))
        return;
}

static int scan_for_macros(FILE *src, MacroTable *macro_table) {
    char line[MAX_LINE_LENGTH], original_line[MAX_LINE_LENGTH], processed_line[MAX_LINE_LENGTH];
    int in_macro_definition = 0, line_num = 0, error = 0;
    Macro *current_macro = NULL;

    while (fgets(line, MAX_LINE_LENGTH, src)) {
        line_num++;

        strcpy(original_line, line);
        strcpy(processed_line, line);
        preprocess_line(processed_line);
    
        if (strlen(processed_line) == 0)
            continue;

        if (in_macro_definition) {
            if (IS_MACRO_END(processed_line)) {
                in_macro_definition = 0;
                current_macro = NULL;
            } else if (current_macro)
                process_macro_body(original_line, current_macro, line_num);
        } else if (IS_MACRO_DEFINITION(processed_line)) {
            if (!process_macro_definition(processed_line, macro_table, &current_macro, &in_macro_definition, line_num))
                error = TRUE;
        }
    }
    
    if (in_macro_definition) {
        print_error("Macro not closed with 'mcroend'", NULL);
        error = TRUE;
    }
    return (error == 0) ? TRUE : FALSE;
}

static int skip_empty_or_comment_line(char *line) {
    char temp[MAX_LINE_LENGTH];

    remove_comments(line);
    strcpy(temp, line);
    trim_whitespace(temp);

    return (strlen(temp) == 0);
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
    size_t length;

    for (i = 0; i < macro->line_count; i++) {
        /* Check if the line itself is not empty after processing */
        length = strlen(macro->body[i]);

        if (macro->body[i] && length > 0)
            fprintf(am, "%s%s%c", indent, macro->body[i], NEWLINE);
    }
}

static int process_macro_call_line(const char *line, FILE *am, const MacroTable *macro_table, int line_num) {
    char *macro_name;
    char indent[MAX_LINE_LENGTH] = "";
    const Macro *macro;

    macro_name = extract_macro_name(line);

    if (!macro_name) {
        fprintf(stderr, "Error: Empty macro name after label at line %d%c", line_num, NEWLINE);
        return FALSE;
    }

    macro = find_macro(macro_table, macro_name);

    if (!macro) {
        fprintf(stderr, "Error: Undefined macro '%s' at line %d%c", macro_name, line_num, NEWLINE);
        return FALSE;
    }

    get_indentation(line, indent);
    write_macro_body(am, macro, indent);

    return TRUE;
}

static int write_am_file(FILE *src, FILE *am, const MacroTable *macro_table) {
    char line[MAX_LINE_LENGTH], temp[MAX_LINE_LENGTH];
    int in_macro_definition = 0, line_num = 0;
    size_t length;

    while (fgets(line, MAX_LINE_LENGTH, src)) {
        line_num++;

        if (skip_empty_or_comment_line(line))
            continue;

        if (in_macro_definition) {
            strcpy(temp, line);
            trim_whitespace(temp);

            if (IS_MACRO_END(temp))
                in_macro_definition = 0;

            continue;
        }

        strcpy(temp, line);
        trim_whitespace(temp);

        if (IS_MACRO_DEFINITION(temp)) {
            in_macro_definition = 1;
            continue;
        }

        if (is_macro_call(temp, macro_table))
            process_macro_call_line(line, am, macro_table, line_num);
        else {
            /* Regular line - write with original spacing preserved */
            length = strlen(line);

            if (length > 0 && line[length - 1] == NEWLINE)
                line[length - 1] = NULL_TERMINATOR;

            fprintf(am, "%s%c", line, NEWLINE);
        }
    }
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int preprocess_macros(const char *filename, const char *am_filename, MacroTable *macro_table) {
    FILE *fp = NULL, *am = NULL;

    fp = open_source_file(filename);

    if (!fp) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!scan_for_macros(fp, macro_table)) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    rewind(fp);

    am = open_output_file(am_filename);

    if (!am) {
        safe_fclose(&fp);
        return PASS_ERROR;
    }

    if (!write_am_file(fp, am, macro_table)) {
        safe_fclose(&fp);
        safe_fclose(&am);
        return PASS_ERROR;
    }

    safe_fclose(&fp);
    safe_fclose(&am);

    return TRUE;
}