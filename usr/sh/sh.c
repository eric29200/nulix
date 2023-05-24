#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pwd.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "../libutils/libutils.h"
#include "readline.h"
#include "cmd.h"

#define USERNAME_SIZE		1024
#define HOSTNAME_SIZE		256

static char username[USERNAME_SIZE];
static char hostname[HOSTNAME_SIZE];
static char cwd[PATH_MAX];

/*
 * Init prompt values.
 */
static void init_prompt_values()
{
	struct passwd *passwd;
	uid_t uid;
	char *s;

	/* reset values */
	memset(username, 0, USERNAME_SIZE);
	memset(hostname, 0, HOSTNAME_SIZE);
	memset(cwd, 0, PATH_MAX);

	/* get hostname */
	gethostname(hostname, HOSTNAME_SIZE - 1);

	/* get user from env */
	s = getenv("USER");
	if (s) 
		return;

	/* get passwd */
	uid = geteuid();
	passwd = getpwuid(uid);
	if (passwd)
		strncpy(username, passwd->pw_name, USERNAME_SIZE);
	else
		snprintf(username, USERNAME_SIZE, "%d", uid);
}

/*
 * Tokenize a string.
 */
static int tokenize(char *str, char **tokens, size_t tokens_len, char *delim)
{
	size_t n = 0;
	char *token;

	token = strtok(str, delim);
	while (token && n < tokens_len) {
		tokens[n++] = token;
		token = strtok(NULL, delim);
	}

	return n;
}

/*
 * Input redirection.
 */
static int input_redirection(char *cmd)
{
	char *tokens[2], *filename;
	int fd;

	if (tokenize(cmd, tokens, 2, "<") <= 1)
		return STDIN_FILENO;

	/* trim filename */
	filename = strtok(tokens[1], " ");

	/* open input file */
	fd = open(filename, O_RDONLY, 0);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	return fd;
}

/*
 * Output redirection.
 */
static int output_redirection(char *cmd)
{
	char *tokens[2], *filename;
	int fd;

	if (tokenize(cmd, tokens, 2, ">") <= 1)
		return STDOUT_FILENO;

	/* trim filename */
	filename = strtok(tokens[1], " ");

	/* open output file */
	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	return fd;
}

/*
 * Execute a command.
 */
static int execute_cmd(char *cmd)
{
	int argc, ret = 0, status;
	char *argv[ARG_MAX];
	int fd_in, fd_out;
	pid_t pid;

	/* input redirection */
	fd_in = input_redirection(cmd);
	if (fd_in < 0)
		return -1;

	/* output redirection */
	fd_out = output_redirection(cmd);
	if (fd_out < 0)
		return -1;

	/* parse arguments (argv must be NULL terminated) */
	argc = tokenize(cmd, argv, ARG_MAX, " ");
	argv[argc] = NULL;

	/* try builtin commands */
	if (cmd_builtin(argc, argv, &ret) == 0)
		goto out;

	/* fork */
	pid = fork();
	if (pid < 0) {
		perror("fork");
		ret = 1;
	} else if (pid == 0) {
		/* redirect stdin */
		if (fd_in != STDIN_FILENO) {
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
		}

		/* redirect stdout */
		if (fd_out != STDOUT_FILENO) {
			dup2(fd_out, STDOUT_FILENO);
			close(fd_out);
		}

		/* execute command */
		ret = execvpe(argv[0], argv, environ);
		if (ret < 0)
			perror(argv[0]);

		/* exit child */
		exit(ret);
	} else {
		/* wait for whild */
		while ((ret = waitpid(pid, &status, 0)) == 0);
		if (ret < 0)
			perror("waitpid");
	}

out:
	/* close redirections */
	if (fd_in != STDIN_FILENO)
		close(fd_in);
	if (fd_out != STDOUT_FILENO)
		close(fd_out);

	return ret;
}

/*
 * Execute a command line.
 */
static int execute_cmdline(char *cmd_line)
{
	int nr_cmds, i, ret = 0;
	char *cmds[ARG_MAX];

	/* parse commands */
	nr_cmds = tokenize(cmd_line, cmds, ARG_MAX, ";");

	/* execute commands */
	for (i = 0; i < nr_cmds; i++)
		ret |= execute_cmd(cmds[i]);

	return ret;
}

/*
 * SIGINT handler.
 */
static void sigint_handler()
{
	printf("\n");
	fflush(stdout);
}

/*
 * Interactive shell.
 */
static int sh_interactive()
{
	char *cmd_line = NULL;
	struct rline_ctx ctx;
	struct tm *tm;
	time_t t;

	/* install signal handlers */
	if (signal(SIGINT, sigint_handler))
		perror("SIGINT");

	/* init prompt */
	init_prompt_values();

	/* init readline context */
	rline_init_ctx(&ctx);

	for (;;) {
		/* get current working directory */
		getcwd(cwd, PATH_MAX);

		/* get current time */
		time(&t);
		tm = localtime(&t);

		/* print prompt */
		printf("[%02d:%02d]\33[1m%s\33[0m@%s:\33[1m%s\33[0m> ", tm->tm_hour, tm->tm_min, username, hostname, cwd);
		fflush(stdout);

		/* get next command */
		if (rline_read_line(&ctx, &cmd_line) <= 0)
			continue;

		/* execute command */
		execute_cmdline(cmd_line);
	}

	/* exit readline context */
	rline_exit_ctx(&ctx);

	return 0;
}

/*
 * Execute shell script.
 */
static int sh_script(const char *filename)
{
	char *cmd_line = NULL;
	size_t n = 0;
	ssize_t len;
	FILE *fp;

	/* open input file */
	fp = fopen(filename, "r");
	if (!fp) {
		perror(filename);
		return 1;
	}
	
	/* execute each line */
	while ((len = getline(&cmd_line, &n, fp)) > 0) {
		/* remove last eol */
		if (cmd_line[len - 1] == '\n')
			cmd_line[len - 1] = 0;

		/* execute command */
		execute_cmdline(cmd_line);
	}

	/* free command line */
	if (cmd_line)
		free(cmd_line);

	/* close input file */
	fclose(fp);

	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s\n", name);
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
	int c;

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

	/* no argument : interactive shell */
	if (!argc)
		return sh_interactive();
	else
		return sh_script(*argv);
}
