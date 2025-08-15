#include "utils.h"
#include "memory.h"
#include "encoder.h"

/* base-4 digit mapping string (a=0, b=1, c=2, d=3) */
static const char base4_digits[BASE4_ENCODING + 1] = {'a','b','c','d',NULL_TERMINATOR};

/* Inner STATIC methods */
/* ==================================================================== */
/*
Function to reverse a string in-place, modifies the input string directly.
Receives: char *str - String to reverse
          int length - Length of the string
*/
static void reverse_string(char *str, int length) {
    char temp;
    int i;

    for (i = 0; i < length / 2; i++) {
        temp = str[i];
        str[i] = str[length - 1 - i];
        str[length - 1 - i] = temp;
    }
}

/* Outer methods */
/* ==================================================================== */
/*
Function to convert a numeric value to base-4 string representation for file headers.
Receives: int value - Numeric value to convert (non-negative)
          char *result - Output buffer (must be large enough)
*/
void convert_to_base4_header(int value, char *result) {
    int i = 0;

    if (value == 0) {
        result[0] = base4_digits[0];
        result[1] = NULL_TERMINATOR;
        return;
    }

    while (value > 0) {
        result[i] = base4_digits[value % BASE4_ENCODING];
        i++;
        value /= BASE4_ENCODING;
    }
    result[i] = NULL_TERMINATOR;
    reverse_string(result, i);
}

/*
Function to convert a memory address to fixed-length base-4 string.
Receives: int value - Address value to convert
          char *result - Output buffer (must be ADDR_LENGTH size)
*/
void convert_to_base4_address(int value, char *result) {
    int i;

    for (i = ADDR_LENGTH - 2; i >= 0; i--) {
        result[i] = base4_digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[ADDR_LENGTH - 1] = NULL_TERMINATOR;
}

/*
Function to convert a 10-bit word value to fixed-length base-4 string.
Receives: int value - Word value to convert (masked to 10 bits)
          char *result - Output buffer (must be WORD_LENGTH size)
*/
void convert_to_base4_word(int value, char *result) {
    int i;

    for (i = WORD_LENGTH - 2; i >= 0; i--) {
        result[i] = base4_digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[WORD_LENGTH - 1] = NULL_TERMINATOR;
}