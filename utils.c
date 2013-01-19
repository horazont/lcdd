#include "utils.h"

#include <stdio.h>
#include <assert.h>

unsigned char unhex(unsigned char hex)
{
    if (hex >= 48 && hex <= 58) {
        return hex - 48;
    } else if (hex >= 97 && hex <= 102) {
        return hex - 87;
    } else if (hex >= 65 && hex <= 70) {
        return hex - 55;
    }
    return 255;
}

int decode_hex(unsigned char *outbuf, const unsigned char *buffer, int len)
{
    unsigned char *out = outbuf;
    const unsigned char *in = buffer;
    const unsigned char *ein = &buffer[(len / 2) * 2];
    while (in < ein) {
        unsigned char tmp = 0;
        unsigned char nibble = unhex(*in);
        in++;
        if (nibble > 0xF) {
            return -1;
        }
        tmp |= nibble << 4;
        nibble = unhex(*in);
        in++;
        if (nibble > 0xF) {
            return -1;
        }
        tmp |= nibble;
        *out = tmp;
        out++;
    }
    return 0;
}

void debug_msg(const char *msg)
{
    fprintf(stderr, "DEBUG: %s", msg);
    fflush(stderr);
}

char *awesomef(const char *message, va_list args, size_t *length)
{
    #define AWESOME_BUFFER_SIZE 256
    char *buffer = (char*)malloc(AWESOME_BUFFER_SIZE);
    size_t actualLength = vsnprintf(buffer, AWESOME_BUFFER_SIZE, message, args);
    if (actualLength >= AWESOME_BUFFER_SIZE) {
        buffer = (char*)realloc(buffer, actualLength+1);
        assert(vsnprintf(buffer, actualLength+1, message, args) == actualLength);
    } else {
        buffer = (char*)realloc(buffer, actualLength+1);
    }
    if (length)
        *length = actualLength;
    return buffer;
}
