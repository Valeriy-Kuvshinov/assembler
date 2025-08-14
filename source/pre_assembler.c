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

static int check_line_length(const char *line, FILE *fp, int line_num) {
    int character;

    if (strlen(line) == MAX_LINE_LENGTH - 1 &&
        line[MAX_LINE_LENGTH - 2] != NEWLINE) {
        print_line_error("Line over 80 characters", NULL, line_num);

        /* Consume the rest of the oversized line to sync the file pointer */
        while (((character = fgetc(fp)) != NEWLINE) && (character != EOF));

        return FALSE;
    }
    return TRUE;
}

static char* extract_macro_name_after_label(char *buffer) {
    char *macro_name_start, *colon_pos;
    size_t length;

    colon_pos = strchr(buffer, LABEL_TERMINATOR);

    if (!colon_pos)
        return NULL;

    macro_name_start = colon_pos + 1;

    while (isspace(*macro_name_start)) {
        macro_name_start++;
    }

    if (*macro_name_start == NULL_TERMINATOR)
        return NULL;

    length = strlen(macro_name_start) + 1;
    memmove(buffer, macro_name_start, length);
    trim_whitespace(buffer);

    return (is_empty_line(buffer) ? NULL : buffer);
}

static char *extract_macro_name(const char *line) {
    static char temp_buffer[MAX_LINE_LENGTH];

    if (!bounded_string_copy(temp_buffer, line, sizeof(temp_buffer), "extract macro name"))
        return NULL;

    if (strchr(temp_buffer, LABEL_TERMINATOR))
        return extract_macro_name_after_label(temp_buffer);
    else {
        /* Handle lines without a label */
        trim_whitespace(temp_buffer);
        return (is_empty_line(temp_buffer) ? NULL : temp_buffer);
    }
}

static int process_macro_definition(char *line, MacroTable *macrotab, Macro **current_macro, int line_num) {
    char *name;
    size_t length;

    length = strlen(MACRO_START);
    name = strtok(line + length, SPACE_TAB);

    if (!name) {
        print_line_error("Missing macro name", NULL, line_num);
        return FALSE;
    }

    if (!is_valid_macro_start(name)) {
        print_line_error(ERR_MACRO, name, line_num);
        return FALSE;
    }

    if (!add_macro(macrotab, name)) {
        print_line_error(ERR_MACRO, name, line_num);
        return FALSE;
    }
    *current_macro = &macrotab->macros[macrotab->count - 1];
    return TRUE;
}

static void process_macro_body(const char *original_line, Macro *current_macro, int line_num) {
    char clean_line[MAX_LINE_LENGTH];

    bounded_string_copy(clean_line, original_line, sizeof(clean_line), "macro body processing");
    preprocess_line(clean_line);

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
        if (macro->body[i] &&
            macro->body[i][0] != NULL_TERMINATOR)
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

static int handle_in_macro_definition(const char *original_line, const char *processed_line, Macro *current_macro, int *in_macro_definition, int line_num) {
    if (IS_MACRO_END(processed_line))
        *in_macro_definition = FALSE;
    else
        process_macro_body(original_line, current_macro, line_num);

    return TRUE;
}

static int handle_outside_macro_definition(const char *original_line, const char *processed_line, FILE *am_fp, MacroTable *macrotab, Macro **current_macro, int *in_macro_definition, int line_num) {
    char temp_line[MAX_LINE_LENGTH];

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

static int process_line(const char *original_line, const char *processed_line, FILE *am_fp, MacroTable *macrotab, Macro **current_macro, int *in_macro_definition, int line_num) {
    if (is_empty_line(processed_line))
        return TRUE; 

    if (*in_macro_definition)
        return handle_in_macro_definition(original_line, processed_line, *current_macro, in_macro_definition, line_num);
    else
        return handle_outside_macro_definition(original_line, processed_line, am_fp, macrotab, current_macro, in_macro_definition, line_num);
}

static int process_file_lines(FILE *src_fp, FILE *am_fp, MacroTable *macrotab) {
    char line[MAX_LINE_LENGTH], original_line[MAX_LINE_LENGTH], processed_line[MAX_LINE_LENGTH];
    int line_num = 0;
    int in_macro_definition = FALSE, has_error = FALSE;
    Macro *current_macro = NULL;

    while (fgets(line, MAX_LINE_LENGTH, src_fp)) {
        line_num++;

        if (!check_line_length(line, src_fp, line_num)) {
            has_error = TRUE;
            continue;
        }

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
    return (has_error ? FALSE : TRUE);
}

/* Outer methods */
/* ==================================================================== */
int preprocess_macros(const char *filename, const char *am_filename, MacroTable *macrotab) {
    int result = TRUE;
    FILE *src_fp = NULL, *am_fp = NULL;

    src_fp = open_source_file(filename);

    if (!src_fp)
        return PASS_ERROR;

    am_fp = open_output_file(am_filename);

    if (!am_fp) {
        safe_fclose(&src_fp);
        return PASS_ERROR;
    }

    if (!process_file_lines(src_fp, am_fp, macrotab))
        result = PASS_ERROR;

    safe_fclose(&src_fp);
    safe_fclose(&am_fp);

    return result;
}