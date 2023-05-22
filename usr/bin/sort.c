#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "../libutils/libutils.h"

#define NR_GROW_LINES		512

static char **lines = NULL;
static size_t lines_capacity = 0;
static size_t nr_lines = 0;
static int reverse = 1;

/*
 * Get all lines of a file.
 */
static int getlines(FILE *fp)
{
	char *line = NULL;
	size_t len, n = 0;
	int ret = 1;

	/* get all lines */
	while (getline(&line, &n, fp) > 0) {
		/* grow lines buffer if needed */
		if (nr_lines + 1 > lines_capacity) {
			lines_capacity += NR_GROW_LINES;
			lines = (char **) realloc(lines, sizeof(char *) * lines_capacity);
			if (!lines)
				goto out;
		}

		/* add a copy of line */
		lines[nr_lines++] = strdup(line);
	}

	/* add '\n' to last line if needed */
	if (lines && nr_lines && lines[nr_lines - 1]) {
		len = strlen(lines[nr_lines - 1]);

		if (lines[nr_lines - 1][len - 1] != '\n') {
			lines[nr_lines - 1] = (char *) realloc(lines[nr_lines - 1], sizeof(char) * (len + 2));
			if (!lines[nr_lines - 1])
				goto out;

			lines[nr_lines - 1][len] = '\n';
			lines[nr_lines - 1][len + 1] = 0;
		}
	}

	ret = 0;
out:
	free(line);
	return ret;
}

/*
 * Compare 2 lines.
 */
static int compar_line(const void *a1, const void *a2)
{
	char *l1 = *((char **) a1);
	char *l2 = *((char **) a2);

	return reverse * strcmp(l1, l2);
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-r] [file ...]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-r, --reverse\t\treverse the result of comparisons\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "reverse",	no_argument,	0,	'r'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int c, ret = 0;
	size_t i;
	FILE *fp;

	/* get options */
	while ((c = getopt_long(argc, argv, "r", long_opts, NULL)) != -1) {
		switch (c) {
			case 'r':
			 	reverse = -1;
				break;
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
	if (!argc) {
		ret = getlines(stdin);
		goto sort;
	}

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
		if (getlines(fp))
			ret = 1;

		/* close file */
		if (fp != stdin && fclose(fp))
			ret = 1;
	}

sort:
	/* sort lines */
	qsort(lines, nr_lines, sizeof(char *), compar_line);

	/* print lines */
	for (i = 0; i < nr_lines; i++)
		printf("%s", lines[i]);

	return ret;
}
