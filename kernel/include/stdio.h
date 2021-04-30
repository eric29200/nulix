#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>
#include <stddef.h>

int sprintf(char *s, const char *format, ...);
int printf(const char *format, ...);
void panic(const char *message);

#endif
