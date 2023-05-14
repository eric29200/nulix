#include <stdlib.h>
#include <unistd.h>
#include <string.h>

char *getenv(const char *name)
{
	size_t len = strchrnul(name, '=') - name;
	char **eptr;

	if (len && !name[len] && environ)
		for (eptr = environ; *eptr; eptr++)
			if (strncmp(name, *eptr, len) == 0 && (*eptr)[len] == '=')
				return *eptr + len + 1;

	return NULL;
}