#ifndef PRE_ASSEMBLER_H
#define PRE_ASSEMBLER_H

#define MAX_MACROS 20  /* Maximum number of macros */
#define MAX_MACRO_NAME_LENGTH 31  /* 30 chars + null terminator */
#define MAX_MACRO_BODY 100  /* Max lines per macro */

#define NUM_INSTRUCTIONS 16

/* Macro Configuration */
#define MACRO_START "mcro"
#define MACRO_END "mcroend"

/* Macro structure */
typedef struct {
    char name[MAX_MACRO_NAME_LENGTH];
    char body[MAX_MACRO_BODY][MAX_LINE_LENGTH];
    int line_count;
} Macro;

/* Macro table to store all macros */
typedef struct {
    Macro macros[MAX_MACROS];
    int count;
} MacroTable;

/* Function prototypes */

int preprocess_macros(const char* src_filename, const char* am_filename);

#endif