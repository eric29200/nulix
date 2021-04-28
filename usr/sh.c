#include <unistd.h>

int main(int argc, char *argv[])
{
  char *tty = "/dev/tty0";
  int fd;

  if (argc == 2) {
    /* open tty 0 */
    fd = open(tty);
    argv[0][1] = 'c';

    if (fd >= 0) {
      /* write to tty0 */
      write(fd, argv[0], 2);
      write(fd, argv[1], 2);

      /* close tty 0 */
      close(fd);
    }
  }

  return 0;
}
