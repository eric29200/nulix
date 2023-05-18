#include <stdio.h>
#include <string.h>
#include <getopt.h>

static char *nextchar = NULL;
static char *optarg = NULL;
static int optind = 1;
static int opterr = 1;
static int optopt = 0;

static int __parse_opt_long(char *const argv[], const struct option *longopts, int *longindex)
{
	char tmp[strlen(nextchar) + 1];
	int found, i;
	char *eq;

	/* copy option string */
	strcpy(tmp, nextchar);

	/* handle '=' */
	eq = strchr(tmp, '=');
	if (eq) {
		*eq = 0;
		optarg = nextchar + (eq - tmp + 1);
	} else {
		optarg = NULL;
	}

	/* try to found long option */
	found = -1;
	for (i = 0; longopts[i].name; i++)
		if (strcmp(longopts[i].name, tmp) == 0)
			found = i;

	/* set option index */
	if (longindex)
		*longindex = -1;

	/* option not found */
	if (found == -1) {
		if (opterr)
			fprintf(stderr, "unrecognized long option: %s\n", tmp);

		nextchar = NULL;
		optind++;
		optopt = 0;
		return '?';
	}
		
	/* set argument if required */
	if (longopts[found].has_arg == required_argument && !optarg)
		optarg = argv[++optind];

	nextchar = NULL;
	optind++;

	/* return value */
	if (!longopts[found].flag)
		return longopts[found].val;

	/* or set flag */
	*longopts[found].flag = longopts[found].val;
	return 0;
}

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
	int optout;
	char *opt;

	/* end of arguments */
	if (optind >= argc)
		return -1;

	while (optind < argc) {
		/* get next option */
		if (!nextchar) {
			nextchar = argv[optind];

			if (*nextchar != '-')
				return -1;
			
			if (!*++nextchar)
				return -1;

			/* try to parse long option */
			if (*nextchar == '-') {
				if (!nextchar[1]) {
					optind++;
					return -1;
				}

				if (longopts) {
					nextchar++;
					return __parse_opt_long(argv, longopts, longindex);
				}
			}
		}
	
	 	/* no option to parse */
		if (!*nextchar) {
			nextchar = NULL;
			optind++;
			continue;
		}

		/* check if option is valid */
		opt = strchr(optstring, *nextchar);
		if (!opt) {
			if (opterr)
				fprintf(stderr, "unrecognized option: %c\n", *nextchar);

			optopt = *nextchar++;
			return '?';
		}

		/* hadle ':' */
		optout = *nextchar;
		if (opt[1] == ':') {
			if (nextchar[1]) {
				optarg = &nextchar[1];
				nextchar = NULL;
				optind++;
			} else {
				optarg = argv[optind + 1];
				nextchar = NULL;
				optind += 2;
			}
		} else {
			nextchar++;
		}

		return optout;
	}

	return -1;
}