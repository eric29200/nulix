#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NTTYS		4
#define KEY_MAP		"/usr/share/keymaps/i386/azerty/fr-latin1.map.gz"

/*
 * Load key map.
 */
static void load_keymap()
{
	pid_t pid;

	pid = fork();
	if (pid == 0)
		execl("/usr/bin/loadkeys", "loadkeys", KEY_MAP, NULL, NULL);
	else
		waitpid(pid, NULL, 0);
}

/*
 * Spwan a shell on tty.
 */
static pid_t spawn_shell(int tty_num)
{
	char tty[32];
	pid_t pid;

	/* set tty */
	sprintf(tty, "/dev/tty%d", tty_num);

	/* create a new process */
	pid = fork();
	if (pid == 0) {
		/* create a new session */
		setsid();

		/* open tty as stdin, stdout, stderr */
		open(tty, O_RDWR, 0);
		dup(0);
		dup(0);

		/* exec a shell */
		if (execl("/usr/bin/bash", "bash", NULL, NULL) == -1)
			exit(0);
	}

	return pid;
}

/*
 * Init process.
 */
int main(void)
{
	pid_t ttys_pid[NTTYS];
	pid_t pid;
	int i;

	/* go to root dir */
	chdir("/");

	/* load key map */
	load_keymap();

	/* spawn a shell on each tty */
	for (i = 0; i < NTTYS; i++)
		ttys_pid[i] = spawn_shell(i + 1);

	/* destroy zombie tasks */
	for (;;) {
		pid = waitpid(-1, NULL, 0);

		/* if main shell exited, respawn it */
		for (i = 0; i < NTTYS; i++)
			if (pid == ttys_pid[i])
				ttys_pid[i] = spawn_shell(i + 1);
	}

	return 0;
}
