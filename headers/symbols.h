#ifndef SYMBOLS_H
#define SYMBOLS_H

/* Various constants */
#define COMMENT_CHAR ';'
#define COMMA_CHAR ','
#define DIRECTIVE_CHAR '.'
#define UNDERSCORE_CHAR '_'
#define LABEL_TERMINATOR ':'
#define IMMEDIATE_PREFIX '#'
#define MATRIX_LEFT_BRACKET '['
#define MATRIX_RIGHT_BRACKET ']'
#define QUOTATION_CHAR '"'

#define REGISTER_CHAR 'r'
#define PROGRAM_STATUS_WORD_REGISTER "PSW"

#define NULL_TERMINATOR '\0'

/* Macro Configuration */
#define MACRO_START "mcro"
#define MACRO_END "mcroend"

/* Data Directives */
#define DATA_DIRECTIVE ".data"
#define STRING_DIRECTIVE ".string"
#define MATRIX_DIRECTIVE ".mat"
/* Symbol Visibility Directives */
#define ENTRY_DIRECTIVE ".entry"
#define EXTERN_DIRECTIVE ".extern"

#endif