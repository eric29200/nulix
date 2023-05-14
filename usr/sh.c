#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pwd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#define USERNAME_SIZE		1024
#define HOSTNAME_SIZE		256
#define CMD_LINE_SIZE		BUFSIZ

static char username[USERNAME_SIZE];
static char hostname[HOSTNAME_SIZE];
static char homepath[PATH_MAX];
static char cwd[PATH_MAX];
static char cmd_line[CMD_LINE_SIZE];

/*
 * Init prompt values.
 */
static void init_prompt_values()
{
	bool user_set = false, home_set = false;
	struct passwd *passwd;
	struct utsname buf;
	uid_t uid;
	char *s;

	/* reset values */
	memset(username, 0, USERNAME_SIZE);
	memset(hostname, 0, HOSTNAME_SIZE);
	memset(homepath, 0, PATH_MAX);
	memset(cwd, 0, PATH_MAX);

	/* get user from env */
	s = getenv("USER");
	if (s) {
		strncpy(username, s, USERNAME_SIZE);
		user_set = true;
	}

	/* get home from env */
	s = getenv("HOME");
	if (s) {
		strncpy(homepath, s, PATH_MAX);
		home_set = true;
	}

	/* get passwd */
	if (!user_set || !home_set) {
		uid = geteuid();
		passwd = getpwuid(uid);

		if (passwd && !user_set) {
			strncpy(username, passwd->pw_name, USERNAME_SIZE);
			user_set = true;
		}

		if (passwd && !home_set) {
			strncpy(homepath, passwd->pw_dir, PATH_MAX);
			home_set = true;
		}
	}

	/* else user uid */
	if (!user_set)
		snprintf(username, USERNAME_SIZE, "%d\n", uid);
	
	/* get hostname */
	if (uname(&buf) == 0)
		strncpy(hostname, buf.nodename, HOSTNAME_SIZE);
}

/*
 * Exit command.
 */
static int __cmd_exit()
{
	exit(0);

	return 0;
}

/*
 * Change dir command.
 */
static int __cmd_cd(int argc, char **argv)
{
	char *path = argc > 1 ? argv[1] : homepath;
	int ret;

	ret = chdir(path);
	if (ret < 0) {
		perror(path);
		return 1;
	}

	return 0;
}

/*
 * Execute a command.
 */
static int execute_cmd()
{
	int argc, ret = 0, status;
	char *argv[ARG_MAX], *s;
	size_t len;
	pid_t pid;

	/* parse arguments */
	for (s = cmd_line, len = 0, argc = 0; *s && argc < ARG_MAX - 1; s++) {
		if (!isspace(*s)) {
			len++;
			continue;
		}

		*s = 0;
		if (len) 
			argv[argc++] = s - len;
		len = 0;
	}

	/* empty command */
	if (!argc)
		return 0;
	
	/* arguments must be NULL terminated */
	argv[argc] = NULL;

	/* builtin commands */
	if (strcmp(argv[0], "exit") == 0) {
		return __cmd_exit();
	} else if (strcmp(argv[0], "cd") == 0) {
		return __cmd_cd(argc, argv);
	}

	/* fork */
	pid = fork();
	if (pid < 0) {
		perror("fork");
		ret = 1;
	} else if (pid == 0) {
		/* execute command */
		ret = execvpe(argv[0], argv, environ);
		if (ret < 0)
			perror(argv[0]);

		/* exit child */
		exit(ret);
	} else {
		/* wait for whild */
		if (waitpid(pid, &status, 0) < 0)
			perror("waitpid");
	}

	return ret;
}

/*
 * Interactive shell.
 */
static int sh_interactive()
{
	/* init prompt */
	init_prompt_values();

	for (;;) {
		/* get current working directory */
		getcwd(cwd, PATH_MAX);

		/* print prompt */
		printf("%s@%s:%s> ", username, hostname, cwd);
		fflush(stdout);

		/* get next command */
		if (!fgets(cmd_line, CMD_LINE_SIZE, stdin))
			continue;

		/* execute command */
		execute_cmd();
	}

	return 0;
}

int main()
{
	return sh_interactive();
}
