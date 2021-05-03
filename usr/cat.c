#include <stdio.h>
#include <unistd.h>

#define CAT_BUF_SIZE      512

int cat(int fd)
{
  char buf[CAT_BUF_SIZE];
  int n;

  while ((n = read(fd, buf, CAT_BUF_SIZE)) > 0) {
    printf("%s", buf);
  }

  return n;
}

int main(int argc, char *argv[])
{
  int fd, i, ret;

  if (argc <= 1)
    return 0;

  for (i = 1; i < argc; i++) {
    fd = open(argv[i]);
    if (fd < 0) {
      return -1;
    }

    ret = cat(fd);
    close(fd);
    if (ret < 0)
      return ret;
  }

  return 0;
}
