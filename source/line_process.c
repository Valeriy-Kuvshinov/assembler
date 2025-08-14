#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "errors.h"
#include "instructions.h"
#include "directives.h"
#include "symbol_table.h"
#include "line_process.h"

int is_directive_line(char **tokens, int token_count) {
    int i;
    
    /* Find the first non-label token */
    for (i = 0; i < token_count; i++) {
        if (!strchr(tokens[i], LABEL_TERMINATOR))
            return (IS_DIRECTIVE(tokens[i]));
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

int check_line_format(char **tokens, int token_count) {
    int has_label = has_label_in_tokens(tokens, token_count);
    int has_directive = is_directive_line(tokens, token_count);
    int has_instruction = is_instruction_line(tokens, token_count);
    
    /* Rule: Cannot have both directive and instruction on same line */
    if (has_directive && has_instruction) {
        print_error("Conflict from directive and instruction on the same line", NULL);
        return FALSE;
    }
    
    /* Rule: Cannot have label alone, must have either directive or instruction */
    if ((has_label && token_count == 1) || (!has_directive && !has_instruction)) {
        print_error("Label must have data directive / instruction after it", NULL);
        return FALSE;
    }
    return TRUE;
}