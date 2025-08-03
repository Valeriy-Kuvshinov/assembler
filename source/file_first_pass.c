#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"
#include "instructions.h"
#include "directives.h"
#include "file_io.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int process_instruction(char **tokens, int token_count, 
                              SymbolTable *symtab, MemoryImage *memory) {
    int operand_start, operand_count, inst_length;
    int inst_idx = 0;
    const Instruction *inst;
    
    if (token_count < 1) 
        return FALSE;
    
    /* Check for label */
    if (strchr(tokens[0], LABEL_TERMINATOR)) {
        if (!process_label(tokens[0], symtab, IC_START + memory->ic, FALSE))
            return FALSE;
        inst_idx = 1;
    }
    
    if (inst_idx >= token_count) {
        print_error("Missing instruction after label", NULL);
        return FALSE;
    }
    
    inst = get_instruction(tokens[inst_idx]);
    /* Find instruction */
    if (!inst) {
        print_error(ERR_UNDEFINED_INSTRUCTION, tokens[inst_idx]);
        return FALSE;
    }

    /* Calculate operand count */
    operand_start = inst_idx + 1;
    operand_count = token_count - operand_start;

    printf("Instruction: %s, Expected operands: %d, Got: %d\n", tokens[inst_idx], inst->num_operands, operand_count);

    /* Validate operand count */
    if (operand_count != inst->num_operands) {
        char error_msg[100];
        sprintf(error_msg, "Expected %d operands, got %d", inst->num_operands, operand_count);
        print_error(ERR_INVALID_OPERAND_COUNT, error_msg);
        return FALSE;
    }
    
    inst_length = calc_instruction_length(inst, tokens + operand_start, operand_count);
    memory->ic += inst_length; /* Update instruction counter */
    
    return TRUE;
}

static int process_directive(char **tokens, int token_count, int start_idx, SymbolTable *symtab, MemoryImage *memory) {
    if (start_idx >= token_count) 
        return FALSE;
    
    if (strcmp(tokens[start_idx], DATA_DIRECTIVE) == 0) 
        return process_data_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], STRING_DIRECTIVE) == 0) 
        return process_string_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], MATRIX_DIRECTIVE) == 0) 
        return process_mat_directive(tokens, token_count, start_idx, symtab, memory);
    
    else if (strcmp(tokens[start_idx], ENTRY_DIRECTIVE) == 0)
        return TRUE; /* Handled in second pass */
    
    else if (strcmp(tokens[start_idx], EXTERN_DIRECTIVE) == 0) {
        if (token_count != start_idx + 2) {
            print_error("Invalid extern directive", NULL);
            return FALSE;
        }
        return add_symbol(symtab, tokens[start_idx + 1], 0, EXTERNAL_SYMBOL);
    }
    
    print_error("Unknown directive", tokens[start_idx]);

    return FALSE;
}

/* Outer regular methods */
/* ==================================================================== */
int first_pass(const char *filename, SymbolTable *symtab, MemoryImage *memory) {
    char line[MAX_LINE_LENGTH];
    int i;
    int line_num = 0, error_flag = 0;
    FILE *fp = fopen(filename, "r");
    
    if (!fp) {
        print_error("Failed to open file", filename);
        return PASS_ERROR;
    }

    /* Initialize - IC starts at 0 (relative to IC_START=100) */
    memory->ic = 0;  
    memory->dc = 0;
    symtab->count = 0;

    /* Initialize memory words */
    for (i = 0; i < WORD_COUNT; i++) {
        memory->words[i].value = 0;
        memory->words[i].are = ARE_ABSOLUTE;
        memory->words[i].ext_symbol_index = -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        char **tokens = NULL;
        int token_count = 0, has_label = 0, directive_idx = 0;
        
        line_num++;
        trim_whitespace(line);
        
        /* Skip empty/comments */
        if (line[0] == NULL_TERMINATOR || line[0] == COMMENT_CHAR)
            continue;

        if (!parse_tokens(line, &tokens, &token_count)) {
            fprintf(stderr, "Line %d: Syntax error\n", line_num);
            error_flag = 1;
            continue;
        }

        if (token_count == 0) {
            free_tokens(tokens, token_count);
            continue;
        }

        /* Check for label */
        if (strchr(tokens[0], LABEL_TERMINATOR)) {
            has_label = 1;
            directive_idx = 1;
        }

        /* Process line */
        if (directive_idx < token_count && tokens[directive_idx][0] == DIRECTIVE_CHAR) {
            /* Handle label for data directive - assign current DC value */
            if (has_label) {
                if (!process_label(tokens[0], symtab, memory->dc, TRUE)) {
                    fprintf(stderr, "Line %d: Label error\n", line_num);
                    error_flag = 1;
                }
            }
            
            if (!process_directive(tokens, token_count, directive_idx, symtab, memory)) {
                fprintf(stderr, "Line %d: Directive error\n", line_num);
                error_flag = 1;
            }
        } else {
            /* Handle instruction - label gets current IC value */
            if (!process_instruction(tokens, token_count, symtab, memory)) {
                fprintf(stderr, "Line %d: Instruction error\n", line_num);
                error_flag = 1;
            }
        }
        free_tokens(tokens, token_count);
    }
    fclose(fp);
    
    /* CRITICAL: Update data symbol addresses to come after code section */
    /* Data symbols get: IC_START + final_IC + their_DC_offset */
    for (i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i].type == DATA_SYMBOL) {
            symtab->symbols[i].value += IC_START + memory->ic;
        }
    }

    /* At the end of first_pass, before returning */
    printf("DEBUG: Final IC = %d, DC = %d\n", memory->ic, memory->dc);

    return error_flag ? PASS_ERROR : PASS_SUCCESS;
}