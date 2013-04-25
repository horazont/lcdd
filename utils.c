#include "utils.h"

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

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

char hex(uint8_t nibble)
{
    static const char map[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    assert(nibble <= 15);
    return map[nibble];
}

void hexbyte(uint8_t byte, char dest[2])
{
    dest[0] = hex((byte & 0xF0) >> 4);
    dest[1] = hex(byte & 0x0F);
}

void print_hex(const uint8_t *buf, size_t len)
{
    const uint8_t *cur = buf;
    const uint8_t *end = buf + len;
    char hexbuf[3];
    memset(hexbuf, 0, sizeof(hexbuf));
    while (cur < end) {
        hexbyte(*cur, hexbuf);
        printf("%s", hexbuf);
        cur++;
    }
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

void encode_hex(char *outbuf, const unsigned char *buffer, int len)
{
    char *out = outbuf;
    const unsigned char *in = buffer;
    const unsigned char *ein = &buffer[len];
    while (in < ein) {
        hexbyte(*in, out);
        out += 2;
        in++;
    }
}

void debug_msg(const char *msg)
{
    fprintf(stderr, "DEBUG: %s", msg);
    fflush(stderr);
}

char *vawesomef(const char *message, va_list args, size_t *length)
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

char *awesomef(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *result = vawesomef(fmt, args, NULL);
    va_end(args);

    return result;
}

char *appendf(char *buffer, int *offset, int *size, const char *fmt, ...)
{
    int fmtd = 0;
    va_list args;
    va_start(args, fmt);

    while (1) {
        fmtd = vsnprintf(&buffer[*offset], *size-*offset,
                         fmt, args);

        if (fmtd >= *size-*offset) {
            int add = 1024;
            if (add < fmtd) {
                add += fmtd;
            }

            char *new = realloc(buffer, *size+1024);
            if (!new) {
                va_end(args);
                return new;
            }
            buffer = new;
            *size += add;
        } else {
            break;
        }
    }
    *offset += fmtd;
    va_end(args);

    return buffer;
}
