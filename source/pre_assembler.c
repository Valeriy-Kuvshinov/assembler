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
/*
Function to check if a line contains only whitespace characters.
Receives: const char *line - The line to check
Returns: int - TRUE if line has only whitespace, FALSE otherwise
*/
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

/*
Function to validate that a line doesn't exceed max of 80 characters.
Receives: const char *line - The line to check
          FILE *fp - Pointer to the source file (for consuming long lines)
          int line_num - Current line number for error reporting
Returns: int - TRUE if line length is valid, FALSE otherwise
*/
static int check_line_length(const char *line, FILE *fp, int line_num) {
    int character;

    if (strlen(line) == MAX_LINE_LENGTH - 1 &&
        line[MAX_LINE_LENGTH - 2] != NEWLINE) {
        print_line_error("Line over 80 characters", NULL, line_num);

        while (((character = fgetc(fp)) != NEWLINE) && (character != EOF)){
            /* Consume the rest of the oversized line to sync the file pointer */
        }
        return FALSE;
    }
    return TRUE;
}

/*
Function to extract the macro name from a line that may contain a label.
Receives: const char *line - The line containing potential macro call
Returns: char* - Pointer to static buffer containing extracted macro name
                (NULL if no valid macro name found)
*/
static char *extract_macro_name(const char *line) {
    static char temp_buffer[MAX_LINE_LENGTH];
    char *macro_name_start, *colon_pos;
    size_t length;

    strcpy(temp_buffer, line);
    colon_pos = strchr(temp_buffer, LABEL_TERMINATOR);

    if (colon_pos) { /* Handle both cases: "LABEL: macro" and "LABEL:macro" */
        macro_name_start = colon_pos + 1;

        while (isspace(*macro_name_start)) {
            macro_name_start++;
        }
        if (*macro_name_start == NULL_TERMINATOR)
            return NULL;

        length = strlen(macro_name_start) + 1;
        memmove(temp_buffer, macro_name_start, length);
        trim_whitespace(temp_buffer);

        if (is_empty_line(temp_buffer))
            return NULL;

        return temp_buffer;
    } else { /* No label, the whole line is the macro name. */
        trim_whitespace(temp_buffer);

        if (is_empty_line(temp_buffer))
            return NULL;

        return temp_buffer;
    }
}

/*
Function to process a macro definition line (starting with 'mcro').
Receives: char *line - The definition line to process
          MacroTable *macrotab - Pointer to macro table
          Macro **current_macro - Pointer to track current macro being defined
          int line_num - Current line number for error reporting
Returns: int - TRUE if definition processed successfully, FALSE otherwise
*/
static int process_macro_definition(char *line, MacroTable *macrotab, Macro **current_macro, int line_num) {
    char *name;
    size_t length;

    if (strchr(line, LABEL_TERMINATOR)) {
        print_line_error("Label not allowed before 'mcro' definition", NULL, line_num);
        return FALSE;
    }
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

/*
Function to process a line of macro, and add it to the body.
Receives: const char *original_line - The unprocessed line
          Macro *current_macro - Pointer to current macro being defined
          int line_num - Current line number for error reporting
*/
static void process_macro_body(const char *original_line, Macro *current_macro, int line_num) {
    char clean_line[MAX_LINE_LENGTH];

    if (strchr(original_line, LABEL_TERMINATOR)) {
        print_line_error("Label not allowed inside a macro", NULL, line_num);
        return;
    }
    strcpy(clean_line, original_line);
    preprocess_line(clean_line);

    if (!add_line_to_macro(current_macro, clean_line))
        print_line_error("Failed to add line to macro", current_macro->name, line_num);
}

/*
Function to extract leading whitespace / tab (indentation) from a line,
to preserve original indentation when expanding macros.
Receives: const char *line - The line to analyze
          char *indent - Output buffer for indentation characters
*/
static void get_indentation(const char *line, char *indent) {
    int i = 0;

    while (line[i] && (line[i] == ' ' || line[i] == '\t')) {
        indent[i] = line[i];
        i++;
    }
    indent[i] = NULL_TERMINATOR;
}

/*
Function to writes a macro's body to .am file,
while preserving original indentation of macro call.
Receives: FILE *am - Pointer to output (.am) file
          const Macro *macro - Pointer to macro being expanded
          const char *indent - Indentation string to preserve
*/
static void write_macro_body(FILE *am, const Macro *macro, const char *indent) {
    int i;

    for (i = 0; i < macro->line_count; i++) {
        if (macro->body[i] &&
            macro->body[i][0] != NULL_TERMINATOR)
            write_file_line(am, indent, macro->body[i]);
    }
}

/*
Function to process a macro call line by expanding the macro contents.
Handles labels and preserves indentation in expansion.
Receives: const char *line - The line containing macro call
          FILE *am - Pointer to output (.am) file
          const MacroTable *macrotab - Pointer to macro table
          int line_num - Current line number for error reporting
Returns: int - TRUE if expansion succeeded, FALSE otherwise
*/
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

/*
Function to handle lines encountered while inside a macro definition.
Processes either macro body content or macro termination.
Receives: const char *original_line - The unprocessed line
          const char *processed_line - The preprocessed line
          Macro *current_macro - Pointer to current macro being defined
          int *in_macro_definition - Flag tracking definition state
          int line_num - Current line number for error reporting
Returns: int - TRUE if processing succeeded, FALSE on error
*/
static int handle_in_macro_definition(const char *original_line, const char *processed_line, Macro *current_macro, int *in_macro_definition, int line_num) {
    if (IS_MACRO_END(processed_line)) {
        if (strchr(original_line, LABEL_TERMINATOR)) {
            print_line_error("Label not allowed before 'mcroend'", NULL, line_num);
            return FALSE;
        }
        *in_macro_definition = FALSE;
    } else
        process_macro_body(original_line, current_macro, line_num);

    return TRUE;
}

/*
Function to handle lines encountered outside macro definitions.
Processes macro definitions, calls, or regular lines, and writes to .am file accordingly.
Receives: const char *original_line - The unprocessed line
          const char *processed_line - The preprocessed line
          FILE *am_fp - Pointer to output (.am) file
          MacroTable *macrotab - Pointer to macro table
          Macro **current_macro - Pointer to track current macro
          int *in_macro_definition - Flag tracking definition state
          int line_num - Current line number for error reporting
Returns: int - TRUE if processing succeeded, FALSE on error
*/
static int handle_outside_macro_definition(const char *original_line, const char *processed_line, FILE *am_fp, MacroTable *macrotab, Macro **current_macro, int *in_macro_definition, int line_num) {
    char temp_line[MAX_LINE_LENGTH];

    if (IS_MACRO_DEFINITION(processed_line)) {
        strcpy(temp_line, processed_line);
        *in_macro_definition = process_macro_definition(temp_line, macrotab, current_macro, line_num);
        return *in_macro_definition;
    }
    if (is_macro_call(processed_line, macrotab))
        return process_macro_call_line(original_line, am_fp, macrotab, line_num);

    fprintf(am_fp, "%s", original_line);
    return TRUE;
}

/*
Function to process a single line during macro preprocessing,
based on current macro definition state.
Receives: const char *original_line - The unprocessed line
          const char *processed_line - The preprocessed line
          FILE *am_fp - Pointer to output (.am) file
          MacroTable *macrotab - Pointer to macro table
          Macro **current_macro - Pointer to track current macro
          int *in_macro_definition - Flag tracking definition state
          int line_num - Current line number for error reporting
Returns: int - TRUE if line processed successfully, FALSE on error
*/
static int process_line(const char *original_line, const char *processed_line, FILE *am_fp, MacroTable *macrotab, Macro **current_macro, int *in_macro_definition, int line_num) {
    if (is_empty_line(processed_line))
        return TRUE; 

    if (*in_macro_definition)
        return handle_in_macro_definition(original_line, processed_line, *current_macro, in_macro_definition, line_num);
    else
        return handle_outside_macro_definition(original_line, processed_line, am_fp, macrotab, current_macro, in_macro_definition, line_num);
}

/*
Function to process all lines from .as file during macro preprocessing.
Manages macro definition state and error tracking.
Receives: FILE *src_fp - Pointer to source (.as) file
          FILE *am_fp - Pointer to output (.am) file
          MacroTable *macrotab - Pointer to macro table
Returns: int - TRUE if all lines processed successfully, FALSE on any error
*/
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
        strcpy(original_line, line);
        strcpy(processed_line, line);
        preprocess_line(processed_line);

        if (!process_line(original_line, processed_line, am_fp, macrotab, &current_macro, &in_macro_definition, line_num))
            has_error = TRUE;
    }
    if (in_macro_definition) {
        print_line_error("Macro not closed with 'mcroend'", NULL, line_num);
        has_error = TRUE;
    }
    return (has_error ? FALSE : TRUE);
}

/* Outer methods */
/* ==================================================================== */
/*
Function to perform the macro preprocessing (first stage) of assembler on an .as file.
Opens file, processes all lines, resolves symbols, and generates output files.
Opens files, processes all lines & macros, and creates expanded output.
Deletes output file if errors occurred during processing.
Receives: const char *filename - Name of source (.as) file
          const char *am_filename - Name for output (.am) file
          MacroTable *macrotab - Pointer to macro table
Returns: int - TRUE if preprocessing succeeded, PASS_ERROR on failure
*/
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

    if (result == PASS_ERROR)
        remove(am_filename);

    return result;
}