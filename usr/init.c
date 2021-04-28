#include <unistd.h>

#define NULL      ((void *) 0)

pid_t spawn_shell()
{
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    execve("/sbin/sh", NULL, NULL);
    return -1;
  }

  return pid;
}

int main(void)
{
  spawn_shell();

  return 0;
}
