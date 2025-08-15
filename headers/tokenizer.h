#ifndef TOKENIZER_H
#define TOKENIZER_H

#define INITIAL_TOKEN_SIZE 32
#define INITIAL_TOKENS_CAPACITY 16

/* Structure to hold parsing state */
typedef struct {
    char *current_token;       /* buffer for current token being built */
    char **tokens;             /* array of completed tokens */
    int in_token;              /* flag indicating if building a token */
    int char_index;            /* position in current_token */
    int token_index;           /* position in tokens array */
    int current_token_size;    /* allocated size of current_token */
    int tokens_capacity;       /* allocated capacity of tokens array */
    int prev_was_comma;        /* flag for comma validation */
    int *token_count;          /* pointer to final token count */
    int in_string;             /* flag to prevent accidental string breakup */
} ParseState;

/* Function prototypes */

int parse_tokens(const char *line, char ***tokens_ptr, int *token_count);

void free_tokens(char **tokens, int token_count);

#endif