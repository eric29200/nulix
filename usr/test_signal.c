#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

void test_handler(int signum)
{
  printf("signal %d received : \n", signum);
}

int main(int argc, char **argv)
{
  struct sigaction act;
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = test_handler;
    sigaction(SIGIO, &act, NULL);
    sleep(10);
    printf("ok1\n");
  } else {
    sleep(5);
    kill(pid, SIGIO);
    printf("ok2\n");
  }

  return 0;
}
