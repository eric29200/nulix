#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#define NTTYS     4

/*
 * Spwan a shell on tty.
 */
pid_t spawn_shell(int tty_num)
{
  char tty[32];
  pid_t pid;

  /* set tty */
  sprintf(tty, "/dev/tty%d", tty_num);

  /* create a new process */
  pid = fork();
  if (pid == 0) {
    /* open tty */
    open(tty, O_RDWR, 0);

    /* dup stdin to sdout and stderr */
    dup(0);
    dup(0);

    execve("/bin/sh", NULL, NULL);
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

  /* set current working dir to root */
  chdir("/");

  /* spawn a shell on each tty */
  for (i = 0; i < NTTYS; i++)
    ttys_pid[i] = spawn_shell(i + 1);

  /* destroy zombie tasks */
  for (;;) {
    pid = wait();

    /* if main shell exited, respawn it */
    for (i = 0; i < NTTYS; i++)
      if (pid == ttys_pid[i])
        ttys_pid[i] = spawn_shell(i + 1);
  }

  return 0;
}
