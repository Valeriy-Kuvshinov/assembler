#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "globals.h"
#include "errors.h"
#include "utils.h"
#include "instructions.h"
#include "pre_assembler.h"

/* Inner STATIC methods */
/* ==================================================================== */

/* Check if macro name is valid */
static int is_valid_macro_name(const char *name) {
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

/* Add new macro to table */
static int add_macro(MacroTable *table, const char *name) {
	int i;

	if (table->count >= MAX_MACROS)
		return FALSE;

	for (i = 0; i < table->count; i++) {
		if (strcmp(table->macros[i].name, name) == 0)
			return FALSE;
	}
	strncpy(table->macros[table->count].name, name, MAX_MACRO_NAME_LENGTH);

	table->macros[table->count].line_count = 0;
	table->count++;

	return TRUE;
}

/* Check if line is macro definition */
static int is_macro_definition(const char *line) {
	char temp[MAX_LINE_LENGTH];

	strcpy(temp, line);
	trim_whitespace(temp);

	return strncmp(temp, MACRO_START, strlen(MACRO_START)) == 0;
}

/* Check if line is macro end */
static int is_macro_end(const char *line) {
	char temp[MAX_LINE_LENGTH];

	strcpy(temp, line);
	trim_whitespace(temp);

	return strcmp(temp, MACRO_END) == 0;
}

/* Find macro by name */
static const Macro *find_macro(const MacroTable *table, const char *name) {
	int i;

	for (i = 0; i < table->count; i++) {
		if (strcmp(table->macros[i].name, name) == 0)
			return &table->macros[i];
	}
	return NULL;
}

/* Check if line is macro call */
static int is_macro_call(const char *line, const MacroTable *table) {
	char temp[MAX_LINE_LENGTH];
	char *token;
	char *colon_pos;

	strcpy(temp, line);
	trim_whitespace(temp);

	colon_pos = strchr(temp, LABEL_TERMINATOR);

	if (colon_pos) {
		token = colon_pos + 1;

		while (isspace((unsigned char) *token))
			token++;

		token = strtok(token, " \t\n");
	} else
		token = strtok(temp, " \t\n");

	if (token)
		return find_macro(table, token) != NULL;

	return FALSE;
}

/* Process macro definition line */
static int process_macro_definition(char *line, MacroTable *macro_table, Macro **current_macro, int *in_macro_definition, int line_number) {
    char *name = strtok(line + strlen(MACRO_START), " \t");
    char *extra = strtok(NULL, " \t\n");
    char context[100];

    if (!name) {
        sprintf(context, "line %d", line_number);
        print_error(ERR_MISSING_MACRO_NAME, context);

        return FALSE;
    }

    if (extra) {
        sprintf(context, "'%s' after macro name '%s' at line %d", extra, name, line_number);
        print_error(ERR_UNEXPECTED_TOKEN, context);

        return FALSE;
    }

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

    *in_macro_definition = 1;
    *current_macro = &macro_table->macros[macro_table->count - 1];

    return TRUE;
}

static void remove_comments(char *line) {
	char *comment_start = strchr(line, COMMENT_CHAR);

	if (comment_start)
		*comment_start = '\0';
}

/* Process macro body line */
static void process_macro_body(const char *original_line, Macro *current_macro, int line_number) {
    if (current_macro->line_count < MAX_MACRO_BODY) {
        char temp_body_line[MAX_LINE_LENGTH];

        strcpy(temp_body_line, original_line);

        remove_comments(temp_body_line);
        trim_whitespace(temp_body_line);

        if (strlen(temp_body_line) > 0) {
            /* Store the cleaned version, not the original */
            sprintf(current_macro->body[current_macro->line_count], "%s\n", temp_body_line);
            current_macro->line_count++;
        }
    } else {
        char context[100];

        sprintf(context, "macro '%s', line %d", current_macro->name, line_number);
        print_error(ERR_MACRO_OVERFLOW, context);
    }
}

/* Collect macro definitions (first pass) */
static int collect_macro_definitions(FILE *src, MacroTable *macro_table) {
	char line[MAX_LINE_LENGTH];
	char original_line[MAX_LINE_LENGTH];
	int in_macro_definition = 0;
	int line_number = 0;
	int error = FALSE;
	Macro *current_macro = NULL;

	while (fgets(line, MAX_LINE_LENGTH, src)) {
		line_number++;
		strcpy(original_line, line);

		remove_comments(line);
		trim_whitespace(line);

		if (in_macro_definition) {
            if (is_macro_end(line)) {
                char *extra = strtok(line + strlen(MACRO_END), " \t\n");

                if (extra) {
                    char context[100];

                    sprintf(context, "'%s' after 'endmcro' at line %d", extra, line_number);
                    print_error(ERR_UNEXPECTED_TOKEN, context);

                    error = TRUE;
                }
                in_macro_definition = 0;
                current_macro = NULL;

                continue;
            }

			if (current_macro)
				process_macro_body(original_line, current_macro, line_number);
		} else {
			if (is_macro_definition(line)) {
				if (!process_macro_definition(line, macro_table, &current_macro, &in_macro_definition, line_number))
					error = TRUE;
			}
		}
	}
    return (error == FALSE) ? TRUE : FALSE;
}

/* Expand macro call */
static void expand_macro_call(FILE *am, const char *processed_line, const MacroTable *macro_table) {
    char temp_line[MAX_LINE_LENGTH];
    char *colon_pos;
    char *macro_name;
    int i;

    strcpy(temp_line, processed_line);
    colon_pos = strchr(temp_line, LABEL_TERMINATOR);

    if (colon_pos) {
        *colon_pos = '\0';
        fprintf(am, "%s:\n", temp_line);  /* Label on its own line */

        macro_name = colon_pos + 1;
        while (isspace((unsigned char)*macro_name)) macro_name++;
        macro_name = strtok(macro_name, " \t\n");
    } else 
        macro_name = strtok(temp_line, " \t\n");

    if (macro_name) {
        const Macro *macro = find_macro(macro_table, macro_name);

        if (macro) {
            /* Write macro body - already has newlines */
            for (i = 0; i < macro->line_count; i++) {
                fputs(macro->body[i], am);
            }
        }
    }
}

/* Expand macros (second pass) */
static int expand_macros(FILE *src, FILE *am, const MacroTable *macro_table) {
	char line[MAX_LINE_LENGTH];
	char processed_line[MAX_LINE_LENGTH];
	int in_macro_definition = 0;

	while (fgets(line, MAX_LINE_LENGTH, src)) {
		strcpy(processed_line, line);

		remove_comments(processed_line);
		trim_whitespace(processed_line);

		if (in_macro_definition) {
			if (is_macro_end(processed_line))
				in_macro_definition = 0;

			continue;
		}

		if (is_macro_definition(processed_line)) {
			in_macro_definition = 1;
			continue;
		}

		if (is_macro_call(processed_line, macro_table)) {
			expand_macro_call(am, processed_line, macro_table);
			continue;
		}

        if (strlen(processed_line) > 0) 
            fprintf(am, "%s\n", processed_line);
	}
	return TRUE;
}

/* Initialize macro table */
static void init_macro_table(MacroTable *table) {
	int i;
	table->count = 0;

	for (i = 0; i < MAX_MACROS; i++) {
		table->macros[i].line_count = 0;
		table->macros[i].name[0] = '\0';
	}
}

/* Outer regular methods */
/* ==================================================================== */

int preprocess_macros(const char *src_filename, const char *am_filename) {
	FILE *src, *am;
	MacroTable macro_table;

	init_macro_table(&macro_table);

	src = fopen(src_filename, "r");

	if (!src) {
		perror("Error opening source file");
		return FALSE;
	}

	/* First pass: collect macro definitions */
	if (!collect_macro_definitions(src, &macro_table)) {
		safe_fclose(&src);
		return FALSE;
	}

	rewind(src); /* Rewind for second pass */

	am = fopen(am_filename, "w"); /* Open output file */

	if (!am) {
		perror("Error creating output file");
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