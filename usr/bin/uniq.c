#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "../libutils/libutils.h"

static char *line_prev = NULL;
static size_t line_prev_len = 0;

/*
 * Print unique lines.
 */
static int uniq(FILE *fp)
{
	char *line = NULL;
	size_t len, n = 0;
	int ret = 1;

	/* get all lines */
	while (getline(&line, &n, fp) > 0) {
		len = strlen(line);

		/* print line if unique */
		if (!line_prev || strcmp(line, line_prev) != 0)
			printf("%s", line);

		/* need to grow previous line */
		if (len > line_prev_len) {
			free(line_prev);
			line_prev = (char *) malloc(len + 1);
			if (!line_prev)
				goto out;
		}

		/* save line */
		strncpy(line_prev, line, len);
	}

	ret = 0;
out:
	free(line);
	return ret;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [file ...]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int c, ret = 0;
	size_t i;
	FILE *fp;

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}

	/* skip options */
	argc -= optind;
	argv += optind;

	/* use stdin */
	if (!argc)
		return uniq(stdin);

	/* wc all files */
	for (i = 0; i < (size_t) argc; i++) {
		/* use stdin or open file */
		if (argv[0][0] == '-') {
			*argv = "<stdin>";
			fp = stdin;
		} else if (!(fp = fopen(*argv, "r"))) {
			perror(*argv++);
			ret = 1;
			continue;
		}

		/* get lines */
		if (uniq(fp))
			ret = 1;

		/* close file */
		if (fp != stdin && fclose(fp))
			ret = 1;
	}

	return ret;
}
