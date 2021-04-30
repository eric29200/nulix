#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>
#include <sys/types.h>

int vsnprintf(char *str, size_t size, const char *format, va_list args);

#endif
