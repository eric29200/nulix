#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	char **env = environ;
	char *eptr;
	size_t len;

	/* print all */
	if (argc == 1) {
		while (*env) {
			eptr = *env++;
			printf("%s\n", eptr);
		}

		return 0;
	}

	/* find variable */
	len = strlen(argv[1]);
	while (*env) {
		if (strlen(*env) > len && env[0][len] == '=' && memcmp(argv[1], *env, len) == 0) {
			eptr = &env[0][len + 1];
			printf("%s\n", eptr);
			return 0;
		}

		env++;
	}

	return 1;
}
