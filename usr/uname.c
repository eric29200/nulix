#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/utsname.h>

#include "libutils/utils.h"

#define FLAG_SYSNAME		(1 << 0)
#define FLAG_NODENAME		(1 << 1)
#define FLAG_RELEASE		(1 << 2)
#define FLAG_VERSION		(1 << 3)
#define FLAG_MACHINE		(1 << 4)

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-snrvma]\n", name);
	fprintf(stderr, "\t  , --help\t\t\tprint help and exit\n");
	fprintf(stderr, "\t-s, --kernel-name\t\tprint the kernel name\n");
	fprintf(stderr, "\t-n, --nodename\t\t\tprint the network node hostname\n");
	fprintf(stderr, "\t-r, --kernel-release\t\tprint the kernel release\n");
	fprintf(stderr, "\t-v, --kernel-version\t\tprint the kernel version\n");
	fprintf(stderr, "\t-m, --machine\t\t\tprint the machine hardware name\n");
	fprintf(stderr, "\t-a, --all\t\t\tprint all information\n");
}
 
/* options */
struct option long_opts[] = {
	{ "help",		no_argument,	0,	OPT_HELP	},
	{ "kernel-name",	no_argument,	0,	's'		},
	{ "nodename",		no_argument,	0,	'n'		},
	{ "kernel-release",	no_argument,	0,	'r'		},
	{ "kernel-version",	no_argument,	0,	'v'		},
	{ "machine",		no_argument,	0,	'm'		},
	{ "all",		no_argument,	0,	'a'		},
	{ 0,			0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	struct utsname buf;
	int flags = 0, c;

	/* get options */
	while ((c = getopt_long(argc, argv, "snrvma", long_opts, NULL)) != -1) {
		switch (c) {
			case 's':
				flags |= FLAG_SYSNAME;
				break;
			case 'n':
				flags |= FLAG_NODENAME;
				break;
			case 'r':
				flags |= FLAG_RELEASE;
				break;
			case 'v':
				flags |= FLAG_VERSION;
				break;
			case 'm':
				flags |= FLAG_MACHINE;
				break;
			case 'a':
				flags |= FLAG_SYSNAME | FLAG_NODENAME | FLAG_RELEASE | FLAG_VERSION | FLAG_MACHINE;
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

	/* check arguments */
	if (argc) {
		usage(name);
		exit(1);
	}

	/* default flag */
	if (!flags)
		flags = FLAG_SYSNAME;

	/* print uname */
	if (uname(&buf) == 0) {
		if (flags & FLAG_SYSNAME)
			printf("%s ", buf.sysname);
		if (flags & FLAG_NODENAME)
			printf("%s ", buf.nodename);
		if (flags & FLAG_RELEASE)
			printf("%s ", buf.release);
		if (flags & FLAG_VERSION)
			printf("%s ", buf.version);
		if (flags & FLAG_MACHINE)
			printf("%s ", buf.machine);
		printf("\n");
	}

	return 0;
}
