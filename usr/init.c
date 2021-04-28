#include <unistd.h>

#define NULL      ((void *) 0)

pid_t spawn_shell()
{
  char *args[] = {"aa", "bb"};
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    execve("/sbin/sh", args, NULL);
    return -1;
  }

  return pid;
}

int main(void)
{
  spawn_shell();

  return 0;
}
