#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "symbols.h"
#include "instructions.h"
#include "directives.h"
#include "label.h"

int validate_label_syntax(const char *label) {
    int i;
    
    /* Check for empty label */
    if (!label || !*label) {
        print_error(ERR_LABEL_EMPTY, NULL);
        return FALSE;
    }
    
    /* Check length */
    if (strlen(label) >= MAX_LABEL_LENGTH) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
    
    /* First character must be alphabetic */
    if (!isalpha((unsigned char)label[0])) {
        print_error(ERR_LABEL_SYNTAX, label);
        return FALSE;
    }
        
    /* Subsequent characters must be alphanumeric or underscore */
    for (i = 1; label[i]; i++) {
        if (!isalnum((unsigned char)label[i]) && label[i] != UNDERSCORE_CHAR) {
            print_error(ERR_LABEL_SYNTAX, label);
            return FALSE;
        }
    }
    
    return TRUE;
}

int is_reserved_word(const char *label) {
    /* Check if label is an instruction */
    if (get_instruction(label) != NULL) {
        print_error(ERR_LABEL_IS_INSTRUCTION, label);
        return TRUE;
    }
    
    /* Check if label is a directive (using defines) */
    if (strcmp(label, DATA_DIRECTIVE) == 0 || 
        strcmp(label, STRING_DIRECTIVE) == 0 || 
        strcmp(label, MATRIX_DIRECTIVE) == 0 || 
        strcmp(label, ENTRY_DIRECTIVE) == 0 || 
        strcmp(label, EXTERN_DIRECTIVE) == 0) {
        print_error(ERR_LABEL_IS_DIRECTIVE, label);
        return TRUE;
    }
    
    /* Check if label is a register */
    if ((label[0] == REGISTER_CHAR && strlen(label) == 2 && 
         isdigit(label[1]) && label[1] >= '0' && label[1] <= '7') ||
        strcmp(label, "PSW") == 0) {
        print_error(ERR_LABEL_IS_REGISTER, label);
        return TRUE;
    }
    
    /* Check if label is a macro keyword (using defines) */
    if (strcmp(label, MACRO_START) == 0 || strcmp(label, MACRO_END) == 0) {
        print_error(ERR_LABEL_IS_RESERVED, label);
        return TRUE;
    }
    
    return FALSE;
}

int is_valid_label(const char *label) {
    /* Basic syntax validation */
    if (!validate_label_syntax(label))
        return FALSE;
    
    /* Check if it's a reserved word */
    if (is_reserved_word(label))
        return FALSE;
    
    return TRUE;
}