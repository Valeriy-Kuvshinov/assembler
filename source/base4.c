#include "utils.h"
#include "memory.h"
#include "symbols.h"
#include "base4.h"

const char digits[BASE4_ENCODING] = {'a', 'b', 'c', 'd'}; /* a=0, b=1, c=2, d=3 */

void convert_to_base4_header(int value, char *result) {
    int j;
    int i = 0;
    
    /* Special case for 0 */
    if (value == 0) {
        result[0] = 'a';
        result[1] = NULL_TERMINATOR;
        return;
    }
    
    /* Convert to base 4 without leading zeros */
    while (value > 0) {
        result[i++] = digits[value % 4];
        value /= 4;
    }
    result[i] = NULL_TERMINATOR;
    
    /* Reverse the string */
    for (j = 0; j < i/2; j++) {
        char temp = result[j];
        result[j] = result[i-1-j];
        result[i-1-j] = temp;
    }
}

void convert_to_base4_address(int value, char *result) {
    int i;
    
    /* Convert to 4-digit base 4 for addresses */
    for (i = ADDR_LENGTH - 2; i >= 0; i--) {
        result[i] = digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[ADDR_LENGTH - 1] = NULL_TERMINATOR;
}

void convert_to_base4_word(int value, char *result) {
    int i;
    
    /* Convert to 5-digit base 4 for machine words */
    for (i = WORD_LENGTH - 2; i >= 0; i--) {
        result[i] = digits[value % BASE4_ENCODING];
        value /= BASE4_ENCODING;
    }
    result[WORD_LENGTH - 1] = NULL_TERMINATOR;
}