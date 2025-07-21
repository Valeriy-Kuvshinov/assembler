#ifndef GLOBALS_H
#define GLOBALS_H

/* Memory Configuration */
#define WORD_COUNT 256 /* Total number of memory words - cells */
#define WORD_MASK 0x3FF /* Mask to enforce 10-bit word size */
#define IC_START 100 /* Instruction Base Address */

/* Register Configuration */
#define NUM_REGISTERS 8 /* r0 to r7 */
#define MAX_REGISTER 7 /* Maximum register index */

/* Symbol Table Configuration */
#define MAX_LABEL_LENGTH 31 /* Max label length (30 + null terminator) */
#define MAX_LABELS 1000 /* Max number of labels in symbol table */

#define CODE_SYMBOL 0
#define DATA_SYMBOL 1
#define EXTERNAL_SYMBOL 2
#define ENTRY_SYMBOL 3

/* A/R/E Field Values */
#define ARE_ABSOLUTE 0
#define ARE_EXTERNAL 1
#define ARE_RELOCATABLE 2

#define BASE4_DIGITS 4
#define BASE4_A_DIGIT "a"
#define BASE4_B_DIGIT "b"
#define BASE4_C_DIGIT "c"
#define BASE4_D_DIGIT "d"

/* File Extensions */
#define FILE_EXT_INPUT ".as"
#define FILE_EXT_PREPROC ".am"
#define FILE_EXT_OBJECT ".ob"
#define FILE_EXT_ENTRY ".ent"
#define FILE_EXT_EXTERN ".ext"
#define FILE_EXT_TEMP ".tmp"

/* Boolean Constants */
#define FALSE 0
#define TRUE !FALSE

#endif