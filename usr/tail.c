#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

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
}

int main(int argc, char **argv)
{
	bool several = false, nl = false;
	const char *name = argv[0];
	size_t nr_lines = 10;
	int ret = 0;
	FILE *fp;

	/* skip arg[0] */
	argc--;
	argv++;

	/* parse options */
	if (argc > 0 && argv[0][0] == '-') {
		nr_lines = atoi(&argv[0][1]);
		if (nr_lines <= 0) {
			usage(name);
			exit(1);
		}

		argc--;
		argv++;
	}

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