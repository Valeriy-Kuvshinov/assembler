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
static char *extract_macro_name_from_call(const char *line) {
    static char temp[MAX_LINE_LENGTH];
    char *macro_name;
    char *colon_pos;

    strcpy(temp, line);
    trim_whitespace(temp);

    colon_pos = strchr(temp, LABEL_TERMINATOR);

    if (colon_pos) {
        /* Handle both cases: "LABEL: macro" and "LABEL:macro" */
        macro_name = colon_pos + 1;
        
        while (isspace(*macro_name)) 
            macro_name++;
            
        /* If we're at end of string, this is invalid (empty macro name) */
        if (*macro_name == NULL_TERMINATOR)
            return NULL;
        
        return macro_name;
    } else
        return temp; /* No label, whole line is macro name */
}

static int parse_macro_declaration(char *line, char **name, int line_num) {
    *name = strtok(line + strlen(MACRO_START), SPACE_TAB);

    if (!*name) {
        fprintf(stderr, "Missing name after 'mcro' declaration, line %d \n", line_num);
        return FALSE;
    }
    return TRUE;
}

static int process_macro_definition(char *line, MacroTable *macro_table, Macro **current_macro, int *in_macro_definition, int line_num) {
    char *name;

    if (!parse_macro_declaration(line, &name, line_num))
        return FALSE;

    if (!is_valid_macro_start(name)) {
        fprintf(stderr, "Invalid macro name '%s', line %d \n", name, line_num);
        return FALSE;
    }

    if (!add_macro(macro_table, name)) {
        fprintf(stderr, "Duplicate macro, line %d \n", line_num);
        return FALSE;
    }
    *in_macro_definition = 1;
    *current_macro = &macro_table->macros[macro_table->count - 1];

    return TRUE;
}

static void process_macro_body(const char *original_line, Macro *current_macro, int line_num) {
    char clean_line[MAX_LINE_LENGTH];
    
    strcpy(clean_line, original_line);
    remove_comments(clean_line);
    trim_whitespace(clean_line);

    /* Add line to macro body (including empty lines) */
    if (current_macro->line_count < MAX_MACRO_BODY) {
        if (strlen(clean_line) > MAX_LINE_LENGTH - 1) {
            clean_line[MAX_LINE_LENGTH - 1] = NULL_TERMINATOR;
            fprintf(stderr, "Warning: Line truncated to 80 chars in macro '%s', line %d \n", 
                    current_macro->name, line_num);
        }
        strcpy(current_macro->body[current_macro->line_count], clean_line);
        current_macro->line_count++;
    } else
        fprintf(stderr, "Macro '%s' body exceeded max size, line %d \n", current_macro->name, line_num);
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

static int write_expanded_output(FILE *src, FILE *am, const MacroTable *macro_table) {
    char line[MAX_LINE_LENGTH], processed_line[MAX_LINE_LENGTH];
    int in_macro_definition = 0;
    int line_num = 0;

    while (fgets(line, MAX_LINE_LENGTH, src)) {
        char temp_check[MAX_LINE_LENGTH];
        line_num++;

        strcpy(processed_line, line);
        
        remove_comments(processed_line);
        
        /* Check if line is empty after removing comments */
        strcpy(temp_check, processed_line);
        trim_whitespace(temp_check);
        
        if (strlen(temp_check) == 0)
            continue;

        if (in_macro_definition) {
            if (IS_MACRO_END(temp_check))
                in_macro_definition = 0;

            continue;
        }

        if (IS_MACRO_DEFINITION(temp_check)) {
            in_macro_definition = 1;
            continue;
        }

        /* Check if this line is a macro call */
        if (is_macro_call(temp_check, macro_table)) {
            char *macro_name = extract_macro_name_from_call(temp_check);
            const Macro *macro;

            /* Add this check for NULL return */
            if (!macro_name) {
                fprintf(stderr, "Error: Empty macro name after label at line %d \n", line_num);
                continue;
            }

            macro = find_macro(macro_table, macro_name);
            
            if (macro) {
                /* Get the indentation of the macro call line */
                char indent[MAX_LINE_LENGTH] = "";
                int i = 0;
                
                /* Extract leading whitespace */
                while (processed_line[i] && (processed_line[i] == ' ' || processed_line[i] == '\t')) {
                    indent[i] = processed_line[i];
                    i++;
                }
                indent[i] = NULL_TERMINATOR;
                
                /* Write macro body with the same indentation */
                for (i = 0; i < macro->line_count; i++) {
                    if (strlen(macro->body[i]) > 0)
                        fprintf(am, "%s%s\n", indent, macro->body[i]);
                }
            } else
                fprintf(stderr, "Error: Undefined macro '%s' at line %d \n", macro_name, line_num);
        } else {
            /* Regular line - write with original spacing preserved */
            int len = strlen(processed_line);

            if (len > 0 && processed_line[len-1] == '\n')
                processed_line[len-1] = NULL_TERMINATOR;
            
            fprintf(am, "%s\n", processed_line);
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

    if (!write_expanded_output(fp, am, macro_table)) {
        safe_fclose(&fp);
        safe_fclose(&am);
        return PASS_ERROR;
    }
    safe_fclose(&fp);
    safe_fclose(&am);

    return TRUE;
}