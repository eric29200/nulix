#include <stdio.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
  struct dirent *entry;
  char *dir_name;
  DIR *dirp;

  /* use arguments or root dir */
  if (argc > 1)
    dir_name = argv[1];
  else
    dir_name = "/";

  /* open directory */
  dirp = opendir(dir_name);
  if (dirp == NULL)
    return -1;

  /* read directory */
  while ((entry = readdir(dirp)) != NULL)
    printf("%s ", entry->d_name);
  printf("\n");

  /* close directory */
  closedir(dirp);

  return 0;
}
