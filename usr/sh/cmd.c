#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

/*
 * Get home directory.
 */
static int get_homedir(char *buf, size_t len)
{
	struct passwd *passwd;
	uid_t uid;
	char *s;

	/* get home from env */
	s = getenv("HOME");
	if (s) {
		strncpy(buf, s, len);
		return 0;
	}

	/* get passwd */
	uid = geteuid();
	passwd = getpwuid(uid);
	if (!passwd)
		return -1;

	/* set homedir */
	strncpy(buf, passwd->pw_dir, len);
	return 0;
 }

/*
 * Change dir command.
 */
static int cmd_cd(int argc, char **argv)
{
	char homedir[PATH_MAX], *path;

	/* use argument or home directory */
	if (argc > 1) {
		path = argv[1];
	} else {
		get_homedir(homedir, PATH_MAX);
		path = homedir;
	}

	/* change directory */
	if (chdir(path) < 0) {
		perror(path);
		return -1;
	}

	return 0;
}
 
/*
 * Execute builtin command.
 */
int cmd_builtin(int argc, char **argv, int *status)
{
	/* exit command */
	if (strcmp(argv[0], "exit") == 0)
		exit(0);

	/* cd command */
	if (strcmp(argv[0], "cd") == 0) {
		*status = cmd_cd(argc, argv);
		return 0;
	}

	return -1;
}