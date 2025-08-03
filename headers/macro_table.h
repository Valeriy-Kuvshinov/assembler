#ifndef MACRO_TABLE_H
#define MACRO_TABLE_H

#define MAX_MACROS 25             /* Maximum number of macros */
#define MAX_MACRO_NAME_LENGTH 31  /* 30 chars + null terminator */
#define MAX_MACRO_BODY 50        /* Max lines per macro */

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

int is_valid_macro_name(const char *name);
int add_macro(MacroTable *table, const char *name);
const Macro *find_macro(const MacroTable *table, const char *name);
int is_macro_definition(const char *line);
int is_macro_end(const char *line);
int is_macro_call(const char *line, const MacroTable *table);
void add_line_to_macro_body(const char *original_line, Macro *current_macro);

#endif