#include "utils.h"
#include "memory.h"
#include "encoder.h"

static const char base4_digits[BASE4_ENCODING + 1] = {'a','b','c','d',NULL_TERMINATOR}; /* a=0, b=1, c=2, d=3 */

/* Inner STATIC methods */
/* ==================================================================== */
static void reverse_string(char *str, int length) {
    int i;

    for (i = 0; i < length / 2; i++) {
        char temp = str[i];
        str[i] = str[length - 1 - i];
        str[length - 1 - i] = temp;
    }
}

/* Outer regular methods */
/* ==================================================================== */
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

void convert_to_base4_address(int value, char *result) {
    int i;

    for (i = ADDR_LENGTH - 2; i >= 0; i--) {
        result[i] = base4_digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[ADDR_LENGTH - 1] = NULL_TERMINATOR;
}

void convert_to_base4_word(int value, char *result) {
    int i;

    for (i = WORD_LENGTH - 2; i >= 0; i--) {
        result[i] = base4_digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[WORD_LENGTH - 1] = NULL_TERMINATOR;
}