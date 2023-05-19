#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>

#include "libutils/utils.h"

#define NR_LINES_DEFAULT	10

/*
 * Tail a file.
 */
static int tail(FILE *fp, const char *filename, size_t nr_lines)
{
	char *line = NULL;
	size_t n = 0, i;
	ssize_t len;

	/* get number of lines */
	for (i = 0; ; i++) {
		len = getline(&line, &n, fp);
		if (len <= 0)
			break;
	}

	/* handle error */
	if (ferror(fp)) {
		fprintf(stderr, "%s: can't read file\n", filename);
		return 1;
	}

	/* rewind file */
	rewind(fp);

	/* print last lines */
	for (;;) {
		/* read next line */
		len = getline(&line, &n, fp);
		if (len <= 0)
			break;

		if (i-- <= nr_lines)
			fwrite(line, 1, len, stdout);
	}

	/* handle error */
	if (ferror(fp)) {
		fprintf(stderr, "%s: can't read file\n", filename);
		return 1;
	}

	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: [-n] [file ...]\n", name);
	fprintf(stderr, "\t  , --help\t\t\tprint help and exit\n");
	fprintf(stderr, "\t-n, --lines=N\t\t\tprint N last lines\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,		0,	OPT_HELP	},
	{ "lines",	required_argument,	0,	'n'		},
	{ 0,		no_argument,		0,	'0'		},
	{ 0,		no_argument,		0,	'1'		},
	{ 0,		no_argument,		0,	'2'		},
	{ 0,		no_argument,		0,	'3'		},
	{ 0,		no_argument,		0,	'4'		},
	{ 0,		no_argument,		0,	'5'		},
	{ 0,		no_argument,		0,	'6'		},
	{ 0,		no_argument,		0,	'7'		},
	{ 0,		no_argument,		0,	'8'		},
	{ 0,		no_argument,		0,	'9'		},
	{ 0,		0,			0,	0		},
};

int main(int argc, char **argv)
{
	bool several = false, nl = false;
	const char *name = argv[0];
	bool nr_lines_set = false;
	size_t nr_lines = 0;
	int ret = 0, c;
	FILE *fp;

	/* get options */
	while ((c = getopt_long(argc, argv, "0123456789n:", long_opts, NULL)) != -1) {
		switch (c) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				nr_lines = nr_lines * 10 + c - '0';
				nr_lines_set = true;
				break;
			case 'n':
			 	nr_lines = atoi(optarg);
				if (nr_lines == 0 && strcmp(optarg, "0") != 0) {
					fprintf(stderr, "%s: incorrect number of lines '%s'\n", name, optarg);
					exit(1);
				}

				nr_lines_set = true;
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

	/* default number or lines */
	if (!nr_lines_set)
		nr_lines = NR_LINES_DEFAULT;

	/* no file : stdin */
	if (!argc)
		return tail(stdin, "<stdin>", nr_lines);

	/* several files ? */
	several = argc > 1;

	/* handle files */
	while (argc-- > 0) {
		/* use stdin or open file */
		if (argv[0][0] == '-') {
			*argv = "<stdin>";
			fp = stdin;
		} else if (!(fp = fopen(*argv, "r"))) {
			perror(*argv++);
			ret = 1;
			continue;
		}

		/* print file name */
		if (several)
			printf("%s==> %s <==\n", nl ? "\n" : "", *argv);

		/* tail */
		ret |= tail(fp, *argv++, nr_lines);
		nl = true;

		/* close file */
		if (fp != stdin && fclose(fp))
			ret = 1;
	}

	return ret;
}
