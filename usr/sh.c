#include <unistd.h>

int main(int argc, char *argv[])
{
  char *prompt = "$";
  int fd;

  /* check number of arguments */
  if (argc != 1)
    return -1;

  /* open tty */
  fd = open(argv[0]);

  /* write prompt to tty */
  if (fd >= 0) {
    write(fd, prompt, 1);

    /* close tty */
    close(fd);
  }

  return 0;
}
