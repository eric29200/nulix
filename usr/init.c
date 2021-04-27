#include <unistd.h>

int main(void)
{
  char *tty = "/dev/tty0";
  char *msg = "Hello";
  int fd;

  /* open tty 0 */
  fd = open(tty);

  /* fork */
  fork();

  if (fd >= 0) {
    /* write to tty0 */
    write(fd, msg, 5);

    /* close tty 0 */
    close(fd);
  }

  return 0;
}
