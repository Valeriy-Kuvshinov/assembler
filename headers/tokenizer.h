#ifndef TOKENIZER_H
#define TOKENIZER_H

#define INITIAL_TOKEN_SIZE 32
#define INITIAL_TOKENS_CAPACITY 16

/* Structure to hold parsing state */
typedef struct {
    char *current_token;
    char **tokens;
    int in_token;
    int char_index;
    int token_index;
    int current_token_size;
    int tokens_capacity;
    int prev_was_comma;
    int *token_count;
} ParseState;

/* Function prototypes */

int parse_tokens(const char *line, char ***tokens_ptr, int *token_count);
void free_tokens(char **tokens, int token_count);

#endif