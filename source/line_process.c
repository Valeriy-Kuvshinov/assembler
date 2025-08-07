#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "errors.h"
#include "instructions.h"
#include "directives.h"
#include "symbol_table.h"
#include "line_process.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int validate_line_components(int has_directive, int has_instruction, int has_label, int token_count) {
    /* Rule: Cannot have both directive and instruction on same line */
    if (has_directive && has_instruction) {
        print_error(ERR_DIR_INSTRUCT_CONFLICT, NULL);
        return FALSE;
    }
    
    /* Rule: Cannot have label alone */
    if (has_label && token_count == 1) {
        print_error(ERR_LABEL_SYNTAX, NULL);
        return FALSE;
    }
    
    /* Rule: Must have either directive or instruction (after preprocessing) */
    if (!has_directive && !has_instruction) {
        print_error(ERR_LABEL_SYNTAX, NULL);
        return FALSE;
    }
    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
int has_label_in_tokens(char **tokens, int token_count) {
    /* Only check first token for label (labels must be first) */
    return (token_count > 0 && strchr(tokens[0], LABEL_TERMINATOR));
}

int is_directive_line(char **tokens, int token_count) {
    int i;
    
    /* Find the first non-label token */
    for (i = 0; i < token_count; i++) {
        if (strchr(tokens[i], LABEL_TERMINATOR) == NULL)
            return (IS_DATA_DIRECTIVE(tokens[i]) || IS_LINKER_DIRECTIVE(tokens[i]));
    }
    return FALSE;
}

int is_instruction_line(char **tokens, int token_count) {
    int i;
    
    /* Find the first non-label token */
    for (i = 0; i < token_count; i++) {
        if (strchr(tokens[i], LABEL_TERMINATOR))
            continue;
        
        return IS_INSTRUCTION(tokens[i]);
    }
    return FALSE;
}

int validate_line_format(char **tokens, int token_count) {
    int has_label = has_label_in_tokens(tokens, token_count);
    int has_directive = is_directive_line(tokens, token_count);
    int has_instruction = is_instruction_line(tokens, token_count);
    
    return validate_line_components(has_directive, has_instruction, has_label, token_count);
}