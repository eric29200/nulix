#include <dirent.h>
#include <unistd.h>

/*
 * Read a directory.
 */
struct dirent *readdir(DIR *dir)
{
  struct dirent *de;

  if (dir->buf_pos >= dir->buf_end) {
    int len = getdents(dir->fd, (struct dirent *) dir->buf, sizeof dir->buf);
    if (len <= 0)
      return 0;

    dir->buf_end = len;
    dir->buf_pos = 0;
  }

  de = (void *)(dir->buf + dir->buf_pos);
  dir->buf_pos += de->d_reclen;
  dir->tell = de->d_off;

  return de;
}
