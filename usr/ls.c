#include <stdio.h>
#include <dirent.h>

int main()
{
  struct dirent *entry;
  DIR *dirp;

  /* open directory */
  dirp = opendir("/");
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
