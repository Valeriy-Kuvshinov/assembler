#ifndef ERRORS_H
#define ERRORS_H

/* Common errors */

/* Memory */
#define ERR_MEMORY_ALLOCATION       "Memory allocation failed"

/* Macro */
#define ERR_MACRO                   "An error occured with a macro"
#define ERR_MACRO_SYNTAX            "Macro must start with letter (a-z / A-Z), may contain integers and underscores later, and be ≤30 chars"

/* Label */
#define ERR_LABEL_SYNTAX            "Label must start with letter (a-z / A-Z), may contain integers later, end with ':', and be ≤30 chars"

/* Directive */
#define ERR_INVALID_MATRIX          "Invalid matrix syntax ([rX][rY]), dimension values must be natural numbers"

#endif