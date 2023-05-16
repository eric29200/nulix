#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
 * Look for a file in path.
 */
static int which(const char *name, const char *path)
{
	bool quit = false;
	char buf[BUFSIZ];
	char *p;

	/* start with first path */
	strncpy(buf, path, BUFSIZ);

	while (!quit) {
		/* find next path end */
		p = strchr(path, ':');
		if (!p)
			quit = true;
		else
			*p = 0;

		/* try concat(path, name) */
		sprintf(buf, "%s/%s", path, name);
		path = ++p;

		/* must be executable */
		if (access(buf, X_OK) == 0) {
			printf("%s\n", buf);
			break;
		}
	}


	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [name ...]\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int i, ret = 0;
	char *path;

	/* check arguments */
	if (argc < 2) {
		usage(name);
		exit(1);
	}

	/* get path */
	path = getenv("PATH");
	if (!path) {
		fprintf(stderr, "PATH is not set\n");
		exit(1);
	}

	/* compute all arguments */
	for (i = 1; i < argc; i++)
		ret |= which(argv[i], path);

	return ret;
}