#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "errors.h"
#include "tokenizer.h"

/* Inner STATIC methods */
/* ==================================================================== */
/*
Function to expand the tokens array when capacity is reached.
Receives: ParseState *state - Current parsing state structure
Returns: int - TRUE if resize succeeded, FALSE on memory error
*/
static int resize_tokens_array(ParseState *state) {
    char **new_tokens;
    int new_capacity;

    new_capacity = state->tokens_capacity * 2;
    new_tokens = realloc(state->tokens, (new_capacity + 1) * sizeof(char *));

    if (!new_tokens) {
        print_error(ERR_MEMORY_ALLOCATION, "Failed to resize tokens array");
        return FALSE;
    }
    state->tokens = new_tokens;
    state->tokens_capacity = new_capacity;

    return TRUE;
}

/*
Function to finalize the current token being processed, and add it to tokens array.
Receives: ParseState *state - Current parsing state structure
Returns: int - TRUE if token was successfully finalized, FALSE on error
*/
static int finalize_token(ParseState *state) {
    if ((state->token_index >= state->tokens_capacity) &&
        (!resize_tokens_array(state)))
        return FALSE;

    state->current_token[state->char_index] = NULL_TERMINATOR;
    state->tokens[state->token_index] = copy_string(state->current_token);

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

/*
Function to resize the current token buffer, if needed during character accumulation.
Receives: char *current_token - Current token buffer
          int char_index - Current position in token
          int *current_token_size - Pointer to current size value
Returns: char* - Resized buffer or NULL on failure
*/
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

/*
Function to finalize the current token, if we're in the middle of processing one.
Receives: ParseState *state - Current parsing state structure
Returns: int - TRUE if not in token or token finalized successfully
*/
static int finalize_if_in_token(ParseState *state) {
    return (!state->in_token || finalize_token(state));
}

/*
Function to handle comma character processing with validation.
Receives: ParseState *state - Current parsing state structure
Returns: int - FALSE if illegal comma position, TRUE otherwise
*/
static int handle_comma(ParseState *state) {
    if (!finalize_if_in_token(state))
        return FALSE;

    if ((state->token_index == 0) || (state->prev_was_comma)) {
        print_error("Illegal comma in tokens", NULL);
        return FALSE;
    }
    state->prev_was_comma = 1;

    return TRUE;
}

/*
Function to process a regular character, adding it to current token.
Receives: ParseState *state - Current parsing state structure
          char cha - Character to process
Returns: int - TRUE if character processed successfully
*/
static int handle_character(ParseState *state, char cha) {
    if (!state->in_token) {
        state->in_token = 1;
        state->prev_was_comma = 0;
    }
    state->current_token = resize_token(state->current_token, state->char_index, &state->current_token_size);

    if (!state->current_token)
        return FALSE;

    state->current_token[state->char_index] = cha;
    state->char_index++;

    return TRUE;
}

/*
Function to handle strings, beginning and end.
Receives: ParseState *state - Current parsing state structure
Returns: int - TRUE if quote processed successfully
*/
static int handle_quote(ParseState *state) {
    if (!handle_character(state, QUOTATION_CHAR))
        return FALSE;

    state->in_string = !state->in_string;

    if (!state->in_string)
        return finalize_token(state);

    return TRUE;
}

/*
Function to handle all character types of parsed tokens.
Receives: ParseState *state - Current parsing state structure
          char cha - Character to process
Returns: int - TRUE if character processed successfully
*/
static int process_character(ParseState *state, char cha) {
    if (state->in_string) {
        if (cha == QUOTATION_CHAR)
            return handle_quote(state);

        return handle_character(state, cha);
    }
    if (isspace(cha))
        return finalize_if_in_token(state);

    else if (cha == COMMA_CHAR)
        return handle_comma(state);

    else if (cha == QUOTATION_CHAR)
        return handle_quote(state);

    else
        return handle_character(state, cha);
}

/*
Function to initialize parsing state structure before processing.
Receives: ParseState *state - State structure to initialize
          int *token_count_ptr - Pointer to token counter
Returns: int - TRUE if initialization succeeded
*/
static int init_parsing(ParseState *state, int *token_count_ptr) {
    state->in_token = 0;
    state->char_index = 0;
    state->token_index = 0;
    state->current_token_size = INITIAL_TOKEN_SIZE;
    state->tokens_capacity = INITIAL_TOKENS_CAPACITY;
    state->prev_was_comma = 0;
    state->in_string = 0;
    state->token_count = token_count_ptr;
    *state->token_count = 0;

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

/*
Function to clean up allocated memory in parsing state structure.
Receives: ParseState *state - State structure to clean up
*/
static void cleanup_parse_state(ParseState *state) {
    safe_free((void**)&state->current_token);
    free_tokens(state->tokens, state->token_index);
}

/* Outer methods */
/* ==================================================================== */
/*
Function to free an array of tokens and all contained strings.
Receives: char **tokens - Array of token strings to free
          int token_count - Number of tokens in array
*/
void free_tokens(char **tokens, int token_count) {
    int i;

    if (!tokens)
        return;

    for (i = 0; i < token_count; i++) {
        safe_free((void**)&tokens[i]);
    }
    safe_free((void**)&tokens);
}

/*
Function to parse a line of input into an array of tokens.
Receives: const char *line - Input line to parse
          char ***tokens_ptr - Output pointer for tokens array
          int *token_count - Output pointer for token count
Returns: int - TRUE if parsing succeeded, FALSE on error
*/
int parse_tokens(const char *line, char ***tokens_ptr, int *token_count) {
    const char *p = line;
    ParseState state;
    *tokens_ptr = NULL; 

    if (!init_parsing(&state, token_count))
        return FALSE;

    while (*p) {
        if (!process_character(&state, *p++)) {
            cleanup_parse_state(&state);
            return FALSE;
        }
    }
    if (!finalize_if_in_token(&state)) {
        cleanup_parse_state(&state);
        return FALSE;
    }
    safe_free((void**)&state.current_token);

    state.tokens[state.token_index] = NULL; 
    *tokens_ptr = state.tokens;

    return TRUE;
}