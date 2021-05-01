#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_SIZE    512
#define MAX_ARGS        32

/*
 * Command structure.
 */
struct cmd_t {
  int argc;
  char *argv[MAX_ARGS];
};

/*
 * Parse a command line.
 */
static void parse_cmd(char *cmd_line, struct cmd_t *cmd)
{
  char *last;
  int i;

  /* parse command line */
  cmd->argc = 0;
  for (i = 0, last = cmd_line; i < MAX_ARGS; i++) {
    cmd->argv[i] = strtok_r(last, " ", &last);
    if (!cmd->argv[i])
      break;
    cmd->argc++;
  }

  /* reset last arguments */
  for (; i < MAX_ARGS; i++)
    cmd->argv[i] = NULL;
}

/*
 * Execute a command.
 */
static int execute_cmd(struct cmd_t *cmd)
{
  pid_t pid;
  int ret;

  /* check number of arguments */
  if (cmd->argc <= 0)
    return -1;

  /* check if file exists */
  ret = access(cmd->argv[0]);
  if (ret != 0)
    return ret;

  /* execute command */
  pid = fork();
  if (pid == 0)
    execve(cmd->argv[0], cmd->argv, NULL);
  else
    wait();

  return 0;
}

/*
 * Main shell function.
 */
int main()
{
  char cmd_line[MAX_CMD_SIZE];
  struct cmd_t cmd;

  for (;;) {
    printf("$");
    gets(cmd_line, MAX_CMD_SIZE);
    parse_cmd(cmd_line, &cmd);
    if (execute_cmd(&cmd) != 0)
      printf("%s : unknown command\n", cmd_line);
  }
  return 0;
}
