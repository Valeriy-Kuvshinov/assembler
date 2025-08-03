#ifndef ERRORS_H
#define ERRORS_H

/* General */
#define ERR_MEMORY_ALLOCATION    "Memory allocation failed"
#define ERR_FILE_OPEN            "Failed to open file"
#define ERR_FILE_WRITE           "Failed to write to file"
#define ERR_LINE_TOO_LONG        "Line exceeds 80 character limit"

/* Macro */
#define ERR_MACRO_OVERFLOW       "Macro body exceeded max size"
#define ERR_MISSING_MACRO_NAME   "Missing macro name"
#define ERR_INVALID_MACRO_NAME   "Macro name cannot be instruction / directive"
#define ERR_DUPLICATE_MACRO      "Duplicate macro name"
#define ERR_MACRO_NOT_CLOSED     "Macro not closed with 'mcroend'"
#define ERR_UNEXPECTED_TOKEN     "Unexpected token in macro definition"

/* Label */
#define ERR_LABEL_SYNTAX         "Label must start with letter (a-z / A-Z) and be ≤30 chars"
#define ERR_LABEL_IS_INSTRUCTION "Label cannot be instruction (mov / cmp / add / ...)" 
#define ERR_LABEL_IS_DIRECTIVE   "Label cannot be directive (.data / .string / ...)"
#define ERR_LABEL_IS_REGISTER    "Label cannot be register (r0-r7 / PSW)"
#define ERR_LABEL_IS_RESERVED    "Label cannot be macro keyword (mcro / mcroend)"
#define ERR_LABEL_MISSING_COLON  "Missing ':' after label"
#define ERR_LABEL_EMPTY          "Empty label (missing name before ':')"
#define ERR_DUPLICATE_LABEL      "Label '%s' already defined"
#define ERR_LABEL_NOT_DEFINED    "Label '%s' not defined"

/* Number / Operand */ 
#define ERR_INVALID_NUMBER       "Invalid number (must be decimal integer)"
#define ERR_NUMBER_RANGE         "Number out of range (-512 - 511)"
#define ERR_INVALID_REGISTER     "Invalid register (must be r0-r7)"
#define ERR_MISSING_HASH         "Immediate value requires '#' prefix"
#define ERR_ILLEGAL_COMMA        "Illegal comma"
#define ERR_CONSECUTIVE_COMMAS   "Consecutive commas"
#define ERR_MISSING_COMMA        "Missing comma between operands"

/* Instruction */
#define ERR_UNKNOWN_INSTRUCTION  "Unknown instruction"
#define ERR_WRONG_OPERAND_COUNT  "Wrong number of operands"
#define ERR_INVALID_ADDRESSING   "Invalid addressing mode for instruction"
#define ERR_TOO_MANY_WORDS       "Instruction exceeds 5-word limit"

/* Directive */
#define ERR_INVALID_DIR_DATA     "Invalid data directive"
#define ERR_INVALID_DIR_STRING   "Invalid string directive"
#define ERR_INVALID_DIR_MAT      "Invalid matrix directive"
#define ERR_INVALID_DIR_ENTRY    "Invalid entry directive"
#define ERR_INVALID_DIR_EXTERN   "Invalid extern directive"

#define ERR_INVALID_DATA         ".data value must be integer (-512 - 511)"
#define ERR_INVALID_STRING       "String must be in double quotes"
#define ERR_INVALID_MATRIX       "Invalid matrix syntax ([rX][rY])"
#define ERR_MISSING_QUOTE        "Missing closing quote for string"
#define ERR_MATRIX_DIM_MISMATCH  "Matrix dimensions don't match values"
#define ERR_ENTRY_NOT_FOUND      ".entry label '%s' not defined"
#define ERR_CONFLICTING_ATTRIB   "Symbol '%s' cannot be both .extern and .entry"

/* Phase */
#define ERR_FIRST_PASS_FAIL      "First pass failed - cannot continue"
#define ERR_SECOND_PASS_FAIL     "Second pass failed - no output generated"

#endif