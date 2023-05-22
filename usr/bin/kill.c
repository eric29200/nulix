#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "../libutils/libutils.h"

/*
 * Signal structure to store number/name.
 */
struct sig {
	int		sig;
	const char *	name;
};

/*
 * Signals.
 */
static struct sig sigs[] = {
	{ 0,			"0"		},
	{ SIGHUP,		"HUP"		},
	{ SIGINT,		"INT"		},
	{ SIGQUIT,		"QUIT"		},
	{ SIGILL,		"ILL"		},
	{ SIGTRAP,		"TRAP"		},
	{ SIGABRT,		"ABRT"		},
	{ SIGBUS,		"BUS"		},
	{ SIGFPE,		"FPE"		},
	{ SIGKILL,		"KILL"		},
	{ SIGUSR1,		"USR1"		},
	{ SIGSEGV,		"SEGV"		},
	{ SIGUSR2,		"USR2"		},
	{ SIGPIPE,		"PIPE"		},
	{ SIGALRM,		"ALRM"		},
	{ SIGTERM,		"TERM"		},
	{ SIGSTKFLT,		"STKFLT"	},
	{ SIGCHLD,		"CHLD"		},
	{ SIGCONT,		"CONT"		},
	{ SIGSTOP,		"STOP"		},
	{ SIGTSTP,		"TSTP"		},
	{ SIGTTIN,		"TTIN"		},
	{ SIGTTOU,		"TTOU"		},
	{ SIGURG,		"URG"		},
	{ SIGXCPU,		"XCPU"		},
	{ SIGXFSZ,		"XFSZ"		},
	{ SIGVTALRM,		"VTALRM"	},
	{ SIGPROF,		"PROF"		},
	{ SIGWINCH,		"WINCH"		},
	{ SIGIO,		"IO"		},
	{ SIGPOLL,		"POLL"		},
	{ SIGPWR,		"PWR"		},
	{ SIGSYS,		"SYS"		},
};

/*
 * Convert a signal number to its name.
 */
static const char *sig2name(int sig)
{
	size_t i;
	
	for (i = 1; i < sizeof(sigs) / sizeof(struct sig); i++)
		if (sig == sigs[i].sig)
			return sigs[i].name;

	return NULL;
}

/*
 * Convert a signal name to its number.
 */
static int name2sig(const char *name)
{
	size_t i;
	
	for (i = 1; i < sizeof(sigs) / sizeof(struct sig); i++)
		if (strcasecmp(name, sigs[i].name) == 0)
			return sigs[i].sig;

	return -1;
}

/*
 * Print all signals.
 */
static void print_signals()
{
	size_t i;

	for (i = 1; i < sizeof(sigs) / sizeof(struct sig); i++)
		printf("%s ", sigs[i].name);

	printf("\n");
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s target name\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-s, --signal=signal\tsignal to send (name or numer)\n");
	fprintf(stderr, "\t-l, --list\tprint a list of signal names\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,		0,	OPT_HELP	},
	{ "list",	no_argument,		0,	'l'		},
	{ "signal",	required_argument,	0,	's'		},
	{ 0,		0,			0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	const char *signame = NULL;
	bool sig_set = false;
	int c, i, sig = 0;
	pid_t pid;

	/* get options */
	while ((c = getopt_long(argc, argv, "0123456789ls:", long_opts, NULL)) != -1) {
		switch (c) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				sig = sig * 10 + c - '0';
				sig_set = true;
				break;
			case 'l':
				print_signals();
			 	exit(0);
				break;
			case 's':
			 	signame = optarg;
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
	if (!argc) {
		usage(name);
		exit(1);
	}

	/* default signal */
	if (!sig_set)
		sig = SIGTERM;

	/* get signal number */
	if (signame) {
		if (isdigit(*signame))
			sig = atoi(signame);
		else
			sig = name2sig(signame);

		/* unknown signal */
		if (sig == -1 || !signame) {
			fprintf(stderr, "unknown signal\n");
			exit(1);
		}
	}

	/* check signal number */
	signame = sig2name(sig);
	if (!signame) {
		fprintf(stderr, "unknown signal\n");
		exit(1);
	}

	/* send signal to each process */
	for (i = 0; i < argc; i++) {
		/* get pid */
		pid = atoi(argv[i]);
		if (!pid) {
			fprintf(stderr, "Bad process number '%s'\n", argv[i]);
			continue;
		}

		/* send signal */
		if (kill(pid, sig) < 0) {
			perror("kill");
			exit(1);
		}
	}

	return 0;
}
