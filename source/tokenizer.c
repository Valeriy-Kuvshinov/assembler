#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"

/* Inner STATIC methods */
/* ==================================================================== */
static int finalize_token(char *current_token, int char_index, char ***tokens,
                          int *token_index, int *token_count, int tokens_capacity) {
    char **temp_tokens = *tokens;

    current_token[char_index] = NULL_TERMINATOR;
    temp_tokens[*token_index] = clone_string(current_token);

    if (!temp_tokens[*token_index]) {
        print_error(ERR_MEMORY_ALLOCATION, NULL);
        return FALSE;
    }
    (*token_index)++;
    (*token_count)++;

    return TRUE;
}

static char *resize_token(char *current_token, int char_index, int *current_token_size) {
    if (char_index >= *current_token_size - 1) {
        *current_token_size *= 2;
        current_token = realloc(current_token, *current_token_size);

        if (!current_token) {
            print_error(ERR_MEMORY_ALLOCATION, NULL);
            return NULL;
        }
    }
    return current_token;
}

static int validate_comma(int token_index, int prev_was_comma) {
    if (token_index == 0 || prev_was_comma) {
        print_error(ERR_ILLEGAL_COMMA, NULL);
        return FALSE;
    }
    return TRUE;
}

static int handle_whitespace(ParseState *state) {
    if (state->in_token) {
        if (!finalize_token(state->current_token, state->char_index, &state->tokens,
                            &state->token_index, state->token_count, state->tokens_capacity))
            return FALSE;

        state->in_token = 0;
        state->char_index = 0;
    }
    return TRUE;
}

static int handle_comma(ParseState *state) {
    if (state->in_token) {
        if (!finalize_token(state->current_token, state->char_index, &state->tokens,
                            &state->token_index, state->token_count, state->tokens_capacity))
            return FALSE;

        state->in_token = 0;
        state->char_index = 0;
    }

    if (!validate_comma(state->token_index, state->prev_was_comma))
        return FALSE;

    state->prev_was_comma = 1;

    return TRUE;
}

static int handle_character(ParseState *state, char c) {
    if (!state->in_token) {
        state->in_token = 1;
        state->prev_was_comma = 0;
    }

    state->current_token = resize_token(state->current_token, state->char_index, &state->current_token_size);

    if (!state->current_token)
        return FALSE;

    state->current_token[state->char_index++] = c;

    return TRUE;
}

static int process_character(ParseState *state, char c) {
    if (isspace(c))
        return handle_whitespace(state);
    else if (c == COMMA_CHAR)
        return handle_comma(state);
    else
        return handle_character(state, c);
}

static int init_parsing(ParseState *state, int *token_count) {
    state->current_token = malloc(state->current_token_size);
    state->tokens = malloc(state->tokens_capacity * sizeof(char *));

    if (!state->current_token || !state->tokens) {
        print_error(ERR_MEMORY_ALLOCATION, NULL);
        safe_free((void**)&state->current_token);
        free_tokens(state->tokens, 0);

        return FALSE;
    }
    *token_count = 0;
    state->token_count = token_count;

    return TRUE;
}

/* Outer regular methods */
/* ==================================================================== */
void free_tokens(char **tokens, int token_count) {
    int i;

    if (!tokens) 
        return;

    for (i = 0; i < token_count; i++) {
        safe_free((void**)&tokens[i]);
    }
    safe_free((void**)&tokens);
}

int parse_tokens(const char *line, char ***tokens_ptr, int *token_count) {
    const char *p = line;
    ParseState state = {0};

    state.in_token = 0;
    state.char_index = 0;
    state.token_index = 0;
    state.current_token_size = INITIAL_TOKEN_SIZE;
    state.tokens_capacity = INITIAL_TOKENS_CAPACITY;
    state.prev_was_comma = 0;

    if (!init_parsing(&state, token_count))
        return FALSE;

    *tokens_ptr = NULL;

    while (*p) {
        if (!process_character(&state, *p)) {
            safe_free((void**)&state.current_token);
            free_tokens(state.tokens, state.token_index);
            return FALSE;
        }
        p++;
    }

    if (state.in_token) {
        if (!finalize_token(state.current_token, state.char_index, &state.tokens,
                            &state.token_index, token_count, state.tokens_capacity)) {
            safe_free((void**)&state.current_token);
            free_tokens(state.tokens, state.token_index);
            return FALSE;
        }
    }
    safe_free((void**)&state.current_token);
    *tokens_ptr = state.tokens;

    return TRUE;
}