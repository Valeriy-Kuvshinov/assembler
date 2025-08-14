#ifndef LINE_PROCESS_H
#define LINE_PROCESS_H

#include "memory.h"
#include "symbol_table.h"

/* Function prototypes */

int is_directive_line(char **tokens, int token_count);

int is_instruction_line(char **tokens, int token_count);

int check_line_format(char **tokens, int token_count);

#endif