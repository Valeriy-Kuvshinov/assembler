#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"

/* Inner STATIC methods */
/* ==================================================================== */
/* New helper function to resize the array of token pointers */
static int resize_tokens_array(ParseState *state) {
    int new_capacity = state->tokens_capacity * 2;
    char **new_tokens = realloc(state->tokens, (new_capacity + 1) * sizeof(char *));

    if (!new_tokens) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize tokens array");
        return FALSE;
    }
    state->tokens = new_tokens;
    state->tokens_capacity = new_capacity;

    return TRUE;
}

static int finalize_token(ParseState *state) {
    if (state->token_index >= state->tokens_capacity) {
        if (!resize_tokens_array(state))
            return FALSE;
    }

    state->current_token[state->char_index] = NULL_TERMINATOR;
    state->tokens[state->token_index] = clone_string(state->current_token);

    if (!state->tokens[state->token_index]) {
        print_error(ERR_MEMORY_ALLOCATION, NULL);
        return FALSE;
    }
    state->token_index++;
    (*state->token_count)++;

    state->in_token = 0;
    state->char_index = 0;

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
        if (!finalize_token(state))
            return FALSE;
    }
    return TRUE;
}

static int handle_comma(ParseState *state) {
    if (state->in_token) {
        if (!finalize_token(state))
            return FALSE;
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

static int init_parsing(ParseState *state, int *token_count_ptr) {
    state->in_token = 0;
    state->char_index = 0;
    state->token_index = 0;
    state->current_token_size = INITIAL_TOKEN_SIZE;
    state->tokens_capacity = INITIAL_TOKENS_CAPACITY;
    state->prev_was_comma = 0;
    state->token_count = token_count_ptr; /* Store pointer to external token_count */
    *state->token_count = 0; /* Initialize external token_count */

    state->current_token = malloc(state->current_token_size);
    state->tokens = malloc((state->tokens_capacity + 1) * sizeof(char *)); 

    if (!state->current_token || !state->tokens) {
        print_error(ERR_MEMORY_ALLOCATION, NULL);
        safe_free((void**)&state->current_token);
        safe_free((void**)&state->tokens);

        return FALSE;
    }
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
    ParseState state;

    if (!init_parsing(&state, token_count))
        return FALSE;

    /* Set tokens_ptr to NULL initially, will be updated on success */
    *tokens_ptr = NULL; 

    while (*p) {
        if (!process_character(&state, *p)) {
            /* Error occurred, clean up all allocated tokens and current_token */
            safe_free((void**)&state.current_token);
            free_tokens(state.tokens, state.token_index); /* token_index is count of tokens successfully added */
            
            return FALSE;
        }
        p++;
    }

    /* Finalize any token that was being built when the line ended */
    if (state.in_token) {
        if (!finalize_token(&state)) {
            safe_free((void**)&state.current_token);
            free_tokens(state.tokens, state.token_index);

            return FALSE;
        }
    }
    /* Free the temporary current_token buffer */
    safe_free((void**)&state.current_token);

    /* Null-terminate the array of token pointers for easy iteration */
    /* This slot was pre-allocated in init_parsing */
    state.tokens[state.token_index] = NULL; 

    /* Pass the allocated tokens array back to the caller */
    *tokens_ptr = state.tokens;

    return TRUE;
}