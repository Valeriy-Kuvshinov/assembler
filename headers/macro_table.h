#ifndef MACRO_TABLE_H
#define MACRO_TABLE_H

#define INITIAL_MACROS_CAPACITY 8
#define MAX_MACRO_NAME_LENGTH 31  /* 30 chars + null terminator */
#define INITIAL_MACRO_BODY_CAPACITY 8

/* Macro open / close keywords */
#define MACRO_START "mcro"
#define MACRO_END "mcroend"

typedef struct {
    char name[MAX_MACRO_NAME_LENGTH];
    char **body;        /* dynamic array of chars of each line */
    int line_count;     /* current number of lines */
    int body_capacity;  /* allocated capacity for body lines */
} Macro;

typedef struct {
    Macro *macros;      /* dynamic array of macros */
    int count;          /* current number of macros */
    int capacity;       /* total allocated macro slots */
} MacroTable;

/* Function prototypes */

int init_macro_table(MacroTable *table);

void free_macro_table(MacroTable *table);

int is_valid_macro_start(const char *name);

const Macro *find_macro(const MacroTable *table, const char *name);

int add_macro(MacroTable *table, const char *name);

int add_line_to_macro(Macro *macro, const char *line_content);

int is_macro_call(const char *line, const MacroTable *table);

/* Validation macros */

#define IS_MACRO_KEYWORD(label) \
    (strcmp((label), MACRO_START) == 0 || strcmp((label), MACRO_END) == 0)

#define IS_MACRO_DEFINITION(line) \
    (strncmp((line), MACRO_START, strlen(MACRO_START)) == 0)

#define IS_MACRO_END(line) \
    (strcmp((line), MACRO_END) == 0)

#endif