#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_SIZE    512
#define PATH_MAX_LEN    512
#define ARGS_MAX        30

/* path environ */
static char *env_path[] = {"/sbin", "/usr/sbin", "/bin", "/usr/bin", NULL};

/*
 * Command structure.
 */
struct cmd_t {
  char path[PATH_MAX_LEN];
  int argc;
  char *argv[ARGS_MAX];
};

/*
 * Parse a command line.
 */
static void parse_cmd(char *cmd_line, struct cmd_t *cmd)
{
  int i;

  /* parse command line */
  for (i = 0; i < ARGS_MAX; i++) {
    cmd->argv[i] = strtok(i == 0 ? cmd_line : NULL, " ");
    if (!cmd->argv[i])
      break;
    cmd->argc++;
  }

  /* reset last arguments */
  for (; i < ARGS_MAX; i++)
    cmd->argv[i] = NULL;
}

/*
 * Find command path.
 */
static int find_cmd_path(struct cmd_t *cmd)
{
  int i, n, filename_len;

  /* file name must be in argv[0] */
  if (cmd->argc <= 0)
    return -1;

  /* if filename contains / do not use environ */
  if (strchr(cmd->argv[0], '/')) {
    strncpy(cmd->path, cmd->argv[0], PATH_MAX_LEN);
    return access(cmd->path, 0);
  }

  /* check environ path */
  filename_len = strlen(cmd->argv[0]);
  for (i = 0; env_path[i] != NULL; i++) {
    /* reset path */
    memset(cmd->path, 0, PATH_MAX_LEN);

    /* check total length */
    n = strlen(env_path[i]);
    if (n + 1 + filename_len >= PATH_MAX_LEN)
      continue;

    /* copy environ path */
    memcpy(cmd->path, env_path[i], n);

    /* add file name */
    cmd->path[n++] = '/';
    strcpy(cmd->path + n, cmd->argv[0]);

    /* check if file exist */
    if (access(cmd->path, 0) == 0)
      return 0;
  }

  return -1;
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

  /* exit command */
  if (strcmp(cmd->argv[0], "exit") == 0)
    exit(0);

  /* cd command */
  if (strcmp(cmd->argv[0], "cd") == 0)
    return chdir(cmd->argv[1]);

  /* find command path */
  ret = find_cmd_path(cmd);
  if (ret != 0)
    return ret;

  /* do not allow to execute init */
  if (strcmp(cmd->path, "/sbin/init") == 0)
    return -1;

  /* execute command */
  pid = fork();
  if (pid == 0 && execve(cmd->path, cmd->argv, NULL) == -1)
    exit(1);
  else
    waitpid(pid, NULL, 0);

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
    fgets(cmd_line, MAX_CMD_SIZE, stdin);
    parse_cmd(cmd_line, &cmd);
    if (execute_cmd(&cmd) != 0)
      printf("%s : unknown command\n", cmd_line);
  }
  return 0;
}
