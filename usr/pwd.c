#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-LP]\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	char buf[PATH_MAX];
	const char *pwd;
	struct stat st;
	int mode = 'L';
	char *p;

	/* skip program name */
	argc--;
	argv++;

	/* parse options */
	while (*argv && argv[0][0] == '-') {
		for (p = &argv[0][1]; *p; p++) {
			switch (*p) {
				case 'L':
				case 'P':
				 	mode = *p;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

	/* get current directory */
	if (!getcwd(buf, PATH_MAX))
		perror(name);

	/* use pwd from env */
	if (mode == 'L' && (pwd = getenv("PWD")) && pwd[0] == '/' && stat(pwd, &st) == 0)
		strncpy(buf, pwd, PATH_MAX);

	/* print */
	printf("%s\n", buf);

	return 0;
}