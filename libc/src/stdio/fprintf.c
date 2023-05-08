#include <stdio.h>
#include <stdarg.h>

int fprintf(FILE *fp, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vfprintf(fp, format, ap);
	va_end(ap);

	return ret;
}