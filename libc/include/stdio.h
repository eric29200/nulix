#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>
#include <sys/types.h>

#define STDIO_BUF_SIZE    512

#define STDIN             0
#define STDOUT            1
#define STDERR            2

int printf(const char *format, ...);

int snprintf(char *str, size_t size, const char *format, ...);
int sprintf(char *str, const char *format, ...);

int vprintf(const char *format, va_list arg);
int vsnprintf(char *str, size_t size, const char *format, va_list args);
int vsprintf(char *str, const char *format, va_list arg);

#endif
