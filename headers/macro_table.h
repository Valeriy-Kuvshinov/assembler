#ifndef MACRO_TABLE_H
#define MACRO_TABLE_H

#define MAX_MACROS 25             /* Maximum number of macros */
#define MAX_MACRO_NAME_LENGTH 31  /* 30 chars + null terminator */
#define MAX_MACRO_BODY 50         /* Max lines per macro */

/* Macro open / close keywords */
#define MACRO_START "mcro"
#define MACRO_END "mcroend"

typedef struct {
    char name[MAX_MACRO_NAME_LENGTH];
    char body[MAX_MACRO_BODY][MAX_LINE_LENGTH];
    int line_count;
} Macro;

typedef struct {
    Macro macros[MAX_MACROS];
    int count;
} MacroTable;

/* Function prototypes */

int is_valid_macro_start(const char *name);

int store_macro(MacroTable *table, const char *name);

const Macro *find_macro(const MacroTable *table, const char *name);

int is_macro_call(const char *line, const MacroTable *table);

/* Validation macros */

#define IS_MACRO_KEYWORD(label) \
    (strcmp((label), MACRO_START) == 0 || strcmp((label), MACRO_END) == 0)

#define IS_MACRO_DEFINITION(line) \
    (strncmp((line), MACRO_START, strlen(MACRO_START)) == 0)

#define IS_MACRO_END(line) \
    (strcmp((line), MACRO_END) == 0)

#endif