#include <stdio.h>
#include <string.h>

char *strrchr(const char *str, int c)
{
	return memrchr(str, c, strlen(str) + 1);
}