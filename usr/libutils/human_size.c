#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "libutils.h"

static const char postfixes[] = "BKMGTPE";

char *human_size(off_t size, char *buf, size_t buflen)
{
	size_t i;
	off_t n;

	for (n = size, i = 0; n >= 1024 && i < strlen(postfixes); i++)
		n /= 1024;

	if (!i)
		snprintf(buf, buflen, "%ju", (uintmax_t) size);
	else
		snprintf(buf, buflen, "%ju%c", n, postfixes[i]);

	return buf;
}
