#include <unistd.h>
#include <stdio.h>

#define MAX_CMD_SIZE    512

int main()
{
  char cmd[MAX_CMD_SIZE];

  for (;;) {
    printf("$");
    gets(cmd, MAX_CMD_SIZE);
    printf("%s : unknown command\n", cmd);
  }
  return 0;
}
