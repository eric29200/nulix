#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
  int fd, i;

  if (argc <= 1)
    return 0;

  for (i = 1; i < argc; i++) {
    fd = open(argv[i], O_WRONLY | O_CREAT, 0);
    if (fd < 0)
      return -1;

    close(fd);
  }

  return 0;
}
