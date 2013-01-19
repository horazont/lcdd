#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdlib.h>

int decode_hex(unsigned char *outbuf, const unsigned char *buffer, int len);
void debug_msg(const char *msg);
char *awesomef(const char *message, va_list args, size_t *length);

#endif
