#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NTTYS     4

/*
 * Spwan a shell on tty.
 */
pid_t spawn_shell(int tty_num)
{
  char tty[32];
  pid_t pid;
  int fd;

  /* set tty */
  sprintf(tty, "/dev/tty%d", tty_num);

  /* create a new process */
  pid = fork();
  if (pid == 0) {
    /* open tty as stdin, stdout, stderr */
    fd = open(tty, O_RDWR, 0);
    dup(0);
    dup(0);

    /* create a new process group (identified by current pid = leader process) */
    pid = getpid();
    setpgid(pid, pid);

    /* mark tty attached to this group */
    tcsetpgrp(fd, pid);

    /* exec a shell */
    if (execl("/bin/sh", "sh", NULL, NULL) == -1)
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
