#ifndef ERRORS_H
#define ERRORS_H

/* Multi purpose errors */

/* Memory */
#define ERR_MEMORY_ALLOCATION       "Memory allocation failed"
#define ERR_CODE_SEGMENT_OVERFLOW \
    "Program too large! Code segment full (%d/%d words used, max %d allowed)", \
    (ic - IC_START), MAX_IC_SIZE, MAX_IC_SIZE

#define ERR_DATA_SEGMENT_OVERFLOW \
    "Data allocation exceeded! Data segment full (%d/%d words used, max %d allowed)", \
    dc, MAX_DC_SIZE, MAX_DC_SIZE

/* Macro */
#define ERR_MACRO_SYNTAX            "Macro must start with letter (a-z / A-Z), may contain integers and underscores later, and be ≤30 chars"
#define ERR_UNEXPECTED_TOKEN        "Unexpected token in macro definition"

/* Label */
#define ERR_LABEL_SYNTAX            "Label must start with letter (a-z / A-Z), may contain integers later, end with ':', and be ≤30 chars"

/* Number / Operand */ 
#define ERR_INVALID_NUMBER          "Invalid number (must be decimal integer)"
#define ERR_MISSING_HASH            "Immediate value requires '#' prefix"
#define ERR_ILLEGAL_COMMA           "Illegal comma"
#define ERR_CONSECUTIVE_COMMAS      "Consecutive commas"
#define ERR_MISSING_COMMA           "Missing comma between operands"

/* Instruction */
#define ERR_MISSING_INSTRUCTION     "Missing instruction after label"
#define ERR_UNKNOWN_INSTRUCTION     "Unknown instruction"
#define ERR_WRONG_OPERAND_COUNT     "Wrong number of operands for instruction"
#define ERR_INVALID_ADDRESSING      "Invalid addressing mode for instruction"
#define ERR_TOO_MANY_WORDS          "Instruction exceeds 5-word limit"

/* Directive */
#define ERR_INVALID_DIR             "Invalid directive"
#define ERR_INVALID_DIR_DATA        "Invalid data directive"
#define ERR_INVALID_DIR_STRING      "Invalid string directive"
#define ERR_INVALID_DIR_MAT         "Invalid matrix directive"
#define ERR_INVALID_DIR_ENTRY       "Invalid entry directive"
#define ERR_INVALID_DIR_EXTERN      "Invalid extern directive"

#define ERR_DIR_INSTRUCT_CONFLICT   "Cannot have both directive and instruction on same line"

#define ERR_INVALID_DATA            ".data value must be integer (-512 - 511)"
#define ERR_INVALID_STRING          "String must be in double quotes"
#define ERR_INVALID_MATRIX          "Invalid matrix syntax ([rX][rY])"
#define ERR_MISSING_QUOTE           "Missing closing quote for string"
#define ERR_MATRIX_DIM_MISMATCH     "Matrix dimensions don't match values"

#endif