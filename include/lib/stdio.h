#ifndef _STDIO_H_
#define _STDIO_H_

#include <lib/stdarg.h>
#include <lib/stddef.h>

#define stdin   0
#define stdout  1
#define stderr  2

int fprintf(int fd, const char *format, ...);
int sprintf(char *s, const char *format, ...);
int printf(const char *format, ...);

#endif
