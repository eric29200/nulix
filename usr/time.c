#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/wait.h>

#include "libutils/opt.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s <program ...>\n", name);
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
	struct timespec start, end;
	time_t secs, msecs;
	int status, c;
	pid_t pid;
	
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
	if (!argc) {
		usage(name);
		exit(1);
	}

	/* get start time */
	clock_gettime(CLOCK_MONOTONIC, &start);

	pid = fork();
	switch (pid) {
		case -1:
			perror("fork");
			exit(1);
			break;
		case 0:
			execvp(*argv, argv);
			perror(*argv);
			exit(1);
			break;
	}

	/* wait for child */
	while (waitpid(pid, &status, 0) != pid)
		continue;

	/* get end time */
	clock_gettime(CLOCK_MONOTONIC, &end);

	/* compute nr of secs and msecs */
	secs = end.tv_sec - start.tv_sec;
	msecs = (end.tv_nsec - start.tv_nsec) / 1000000L;

	/* print result */
	fprintf(stderr, "%ld.%03lds\n", secs, msecs);

	return !!status;
}
