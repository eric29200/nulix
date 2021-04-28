#include <unistd.h>

#define NULL      ((void *) 0)

/*
 * Spawn a shell on a tty.
 */
pid_t spawn_shell(char *tty)
{
  char *args[] = {tty};
  pid_t pid;

  /* fork and execute a shell */
  pid = fork();
  if (pid == 0) {
    execve("/sbin/sh", args, NULL);
    return -1;
  }

  return pid;
}

int main(void)
{
  /* spawn a shell on each tty */
  if (spawn_shell("/dev/tty0") == -1)
    return -1;
  if (spawn_shell("/dev/tty1") == -1)
    return -1;
  if (spawn_shell("/dev/tty2") == -1)
    return -1;
  if (spawn_shell("/dev/tty3") == -1)
    return -1;

  for (;;);

  return 0;
}
