#include <stdio.h>
#include <unistd.h>

int main()
{
  char buf[256];

  getcwd(buf, 256);
  printf("%s\n", buf);

  return 0;
}
