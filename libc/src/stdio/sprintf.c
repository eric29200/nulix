#include <stdio.h>

int sprintf(char *str, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vsprintf(str, format, ap);
	va_end(ap);

	return ret;
}
