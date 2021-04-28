#include <unistd.h>

#define NULL      ((void *) 0)

int main(void)
{
  char *args[] = {"/dev/tty0"};
  pid_t pid;

  /* spawn a shell on first tty */
  pid = fork();
  if (pid == 0) {
    execve("/sbin/sh", args, NULL);
    return -1;
  }

  return 0;
}
