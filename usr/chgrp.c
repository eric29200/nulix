#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h>
#include <sys/stat.h>
#include <getopt.h>

#include "libutils/opt.h"

/*
 * Get gid from decimal string.
 */
static void __gid_from_decimal(const char *gid_str, gid_t *gid)
{
	*gid = 0;

	while (*gid_str >= '0' && *gid_str <= '9')
		*gid = *gid * 10 + (*gid_str++ - '0');

	if (*gid_str) {
		fprintf(stderr, "Bad gid value\n");
		exit(1);
	}
}

/*
 * Get gid from name.
 */
static void __gid_from_name(const char *name, gid_t *gid)
{
	struct group *grp;

	grp = getgrnam(name);
	if (!grp) {
		fprintf(stderr, "%s : unknown group\n", name);
		exit(1);
	}

	*gid = grp->gr_gid;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s { group_name | group_id } file [...]\n", name);
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
	struct stat statbuf;
	gid_t gid;
	int i, c;
	char *s;

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

	/* check arguments */
	if (argc < 2) {
		usage(name);
		exit(1);
	}

	/* get gid */
	s = argv[0];
	if (*s >= '0' && *s <= '9')
		__gid_from_decimal(s, &gid);
	else
		__gid_from_name(s, &gid);

	/* chown arguments */
	for (i = 1; i < argc; i++)
		if (stat(argv[i], &statbuf) < 0 || chown(argv[i], statbuf.st_uid, gid) < 0)
			perror(argv[i]);

	return 0;
}
