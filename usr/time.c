#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s <program ...>\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	struct timespec start, end;
	time_t secs, msecs;
	int status;
	pid_t pid;

	/* check arguments */
	if (argc < 2) {
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
			execvp(argv[1], &argv[1]);
			perror(argv[1]);
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
