#include <unistd.h>
#include <string.h>

#define NULL      ((void *) 0)

/*
 * Spwan a shell on tty.
 */
void spawn_shell(char *tty)
{
  char *args[] = {tty};
  pid_t pid;

  pid = fork();
  if (pid == 0)
    execve("/sbin/sh", args, NULL);
}

int main(void)
{
  char *msg = "aab";
  int fd;

  sleep(5);

  fd = open("/dev/tty0");
  write(fd, msg, 3);
  close(fd);
  sleep(10);
  fd = open("/dev/tty0");
  write(fd, msg, 3);
  close(fd);

  //spawn_shell("/dev/tty0");
  //spawn_shell("/dev/tty1");
  //spawn_shell("/dev/tty2");
  //spawn_shell("/dev/tty3");

  return 0;
}
