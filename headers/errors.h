#ifndef ERRORS_H
#define ERRORS_H

/* Error codes */
#define ERR_MEMORY_ALLOCATION "Memory allocation failed"
#define ERR_ILLEGAL_COMMA "Illegal comma"
#define ERR_CONSECUTIVE_COMMAS "Consecutive commas"
#define ERR_MACRO_OVERFLOW "Macro body exceeded maximum size"
#define ERR_INVALID_FILENAME "Input filename must not have an extension"
#define ERR_MISSING_MACRO_NAME "Missing macro name"
#define ERR_INVALID_MACRO_NAME "Invalid macro name"
#define ERR_DUPLICATE_MACRO "Duplicate macro name"
#define ERR_UNEXPECTED_TOKEN "Unexpected token"
#define ERR_UNDEFINED_INSTRUCTION "Undefined instruction"
#define ERR_INVALID_OPERAND_COUNT "Incorrect number of operands"
#define ERR_ILLEGAL_ADDRESSING_MODE "Illegal addressing mode"
#define ERR_LABEL_SYNTAX "Invalid label syntax"
#define ERR_LABEL_TOO_LONG "Label name too long"
#define ERR_DUPLICATE_LABEL "Label already defined"
#define ERR_UNKNOWN_LABEL "Label not found"
#define ERR_MISSING_LABEL_COLON "Missing ':' after label"
#define ERR_ENTRY_LABEL_NOT_FOUND "Entry label not defined"

#endif