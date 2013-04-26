#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

int decode_hex(unsigned char *outbuf, const unsigned char *buffer, int len);
void encode_hex(char *outbuf, const unsigned char *buffer, int len);
void debug_msg(const char *msg);
char *vawesomef(const char *message, va_list args, size_t *length);
char *awesomef(const char *fmt, ...);
char *appendf(char *buffer, int *offset, int *size, const char *fmt, ...);

uint8_t adler8ish(const uint8_t *buffer, size_t len);

ssize_t block_read(int fd, void *buf, size_t count);

#endif
