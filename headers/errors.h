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
#define ERR_MISSING_HASH            "Immediate value requires '#' prefix"
#define ERR_ILLEGAL_COMMA           "Illegal comma"
#define ERR_CONSECUTIVE_COMMAS      "Consecutive commas"
#define ERR_MISSING_COMMA           "Missing comma between operands"

/* Instruction */
#define ERR_TOO_MANY_WORDS          "Instruction exceeds 5-word limit"

/* Directive */
#define ERR_INVALID_MATRIX          "Invalid matrix syntax ([rX][rY])"

#endif