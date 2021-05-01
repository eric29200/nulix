#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define NTTYS     4

/*
 * Spwan a shell on tty.
 */
void spawn_shell(int tty_num)
{
  char tty[32];
  pid_t pid;

  /* set tty */
  sprintf(tty, "/dev/tty%d", tty_num);

  /* create a new process */
  pid = fork();
  if (pid == 0) {
    /* open tty */
    open(tty);

    /* dup stdin to sdout and stderr */
    dup(0);
    dup(0);

    execve("/bin/sh", NULL, NULL);
  }
}

/*
 * Init process.
 */
int main(void)
{
  int i;

  /* spawn a shell on each tty */
  for (i = 0; i < NTTYS; i++)
    spawn_shell(i + 1);

  /* destroy zombie tasks */
  for (;;)
    wait();

  return 0;
}
