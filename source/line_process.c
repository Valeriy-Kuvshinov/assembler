#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "errors.h"
#include "instructions.h"
#include "directives.h"
#include "symbol_table.h"
#include "line_process.h"

/*
Function to check if a tokenized line contains a directive (prefix '.').
Receives: char **tokens - Array of tokens from line
          int token_count - Number of tokens in array
Returns: int - TRUE if line contains valid directive, FALSE otherwise
*/
int is_directive_line(char **tokens, int token_count) {
    int i;
    
    for (i = 0; i < token_count; i++) {
        if (!strchr(tokens[i], LABEL_TERMINATOR))
            return (IS_DIRECTIVE(tokens[i]));
    }
    return FALSE;
}

/*
Function to check if a tokenized line contains an instruction from collection.
Receives: char **tokens - Array of tokens from line
          int token_count - Number of tokens in array
Returns: int - TRUE if line contains valid instruction, FALSE otherwise
*/
int is_instruction_line(char **tokens, int token_count) {
    int i;
    
    for (i = 0; i < token_count; i++) {
        if (strchr(tokens[i], LABEL_TERMINATOR))
            continue;
        
        return IS_INSTRUCTION(tokens[i]);
    }
    return FALSE;
}

/*
Function to validate basic syntax structure of an .am line.
Rules:
- Prohibits mixing directives and instructions
- Requires content after label definition
- Requires either directive or instruction
Receives: char **tokens - Tokenized line to validate
          int token_count - Number of tokens
          int line_num - Source line number for error reporting
Returns: int - TRUE if valid format, FALSE with error message if invalid
*/
int check_line_format(char **tokens, int token_count, int line_num) {
    int has_label = is_first_token_label(tokens, token_count);
    int has_directive = is_directive_line(tokens, token_count);
    int has_instruction = is_instruction_line(tokens, token_count);

    if (has_directive && has_instruction) {
        print_line_error("Cannot have directive & instruction on same line", NULL, line_num);
        return FALSE;
    }
    if (has_label && token_count == 1) {
        print_line_error("Label must have directive / instruction after it", tokens[0], line_num);
        return FALSE;
    }
    if (!has_directive && !has_instruction) {
        print_line_error("Line must contain directive / instruction", NULL, line_num);
        return FALSE;
    }
    return TRUE;
}