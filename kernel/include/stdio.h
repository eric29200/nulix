#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>
#include <stddef.h>

int sprintf(char *s, const char *format, ...);
int snprintf(char *s, size_t len, const char *format, ...);
int printf(const char *format, ...);
void panic(const char *message);
int sys_syslog(int type, char *buf, int len);

#endif
