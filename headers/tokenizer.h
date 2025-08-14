#ifndef TOKENIZER_H
#define TOKENIZER_H

#define INITIAL_TOKEN_SIZE 32
#define INITIAL_TOKENS_CAPACITY 16

/* Structure to hold parsing state */
typedef struct {
    char *current_token;       /* Buffer for current token being built */
    char **tokens;             /* Array of completed tokens */
    int in_token;              /* Flag indicating if building a token */
    int char_index;            /* Position in current_token */
    int token_index;           /* Position in tokens array */
    int current_token_size;    /* Allocated size of current_token */
    int tokens_capacity;       /* Allocated capacity of tokens array */
    int prev_was_comma;        /* Flag for comma validation */
    int *token_count;          /* Pointer to final token count */
} ParseState;

/* Function prototypes */

int parse_tokens(const char *line, char ***tokens_ptr, int *token_count);

void free_tokens(char **tokens, int token_count);

#endif