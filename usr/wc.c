#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define FLAG_NR_CHARS		(1 << 0)
#define FLAG_NR_WORDS		(1 << 1)
#define FLAG_NR_LINES		(1 << 2)

/* total counts */
static int tc = 0, tw = 0, tl = 0;

/*
 * Print result.
 */
static void print_result(const char *filename, size_t nc, size_t nw, size_t nl, int flags)
{
	/* print number of lines */
	if (flags & FLAG_NR_LINES)
		printf("\t%u", nl);

	/* print number of words */
	if (flags & FLAG_NR_WORDS)
		printf("\t%u", nw);

	/* print number of chars */
	if (flags & FLAG_NR_CHARS)
		printf("\t%u", nc);

	/* print file name */
	if (filename)
		printf("\t%s", filename);

	/* end line */
	printf("\n");
}

/*
 * Count a file.
 */
static int wc(FILE *fp, const char *filename, int flags)
{
	size_t nc = 0, nw = 0, nl = 0, len;
	bool word = false;
	char c;

	while ((len = fread(&c, 1, 1, fp)) > 0) {
		/* update number of characters */
		nc++;

		/* update number of words */
		if (!isspace(c)) {
			word = true;
		} else if (word) {
			word = false;
			nw++;
		}

		/* update number of lines */
		if (c == '\n')
			nl++;
	}

	/* handle error */
	if (ferror(fp))
		return 1;

	/* adjust number of words */
	if (word)
		nw++;

	/* print result */
	print_result(filename, nc, nw, nl, flags);

	/* update total */
	tc += nc;
	tw += nw;
	tl += nl;

	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-cwl] [file ...]\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int flags = 0, ret = 0;
	bool several = false;
	FILE *fp;
	char *p;

	/* parse options */
	while (argv[1] && argv[1][0] == '-') {
		for (p = &argv[1][1]; *p; p++) {
			switch (*p) {
				case 'c':
					flags |= FLAG_NR_CHARS;
					break;
				case 'w':
					flags |= FLAG_NR_WORDS;
					break;
				case 'l':
					flags |= FLAG_NR_LINES;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

	/* default flags */
	if (!flags)
		flags = FLAG_NR_CHARS | FLAG_NR_WORDS | FLAG_NR_LINES;

	/* update argument position */
	argv++;
	if (--argc == 0)
		return wc(stdin, NULL, flags);

	/* several files ? */
	several = argc > 1;
	
	/* wc all files */
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

		/* count */
		ret |= wc(fp, *argv++, flags);
	}

	/* print total */
	if (several)
		print_result("total", tc, tw, tl, flags);

	return ret;
}