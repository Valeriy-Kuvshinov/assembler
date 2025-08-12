#ifndef ERRORS_H
#define ERRORS_H

/* Common errors */

/* Memory */
#define ERR_MEMORY_ALLOCATION       "Memory allocation failed"

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

/* Directive */
#define ERR_INVALID_MATRIX          "Invalid matrix syntax ([rX][rY]), dimension values must be natural numbers"

#endif