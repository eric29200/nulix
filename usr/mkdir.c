#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int i;

  if (argc <= 1)
    return 0;

  for (i = 1; i < argc; i++)
    if (mkdir(argv[i]) < 0)
      return -1;

  return 0;
}
