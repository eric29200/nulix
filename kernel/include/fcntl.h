#ifndef _FCNTL_H_
#define _FCNTL_H_

#define S_IFMT        00170000
#define S_IFREG       0100000
#define S_IFBLK       0060000
#define S_IFDIR       0040000
#define S_IFCHR       0020000
#define S_IFIFO       0010000
#define S_ISUID       0004000
#define S_ISGID       0002000
#define S_ISVTX       0001000

#define S_ISREG(m)	  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	  (((m) & S_IFMT) == S_IFIFO)

#define S_IRWXU       00700
#define S_IRUSR       00400
#define S_IWUSR       00200
#define S_IXUSR       00100

#define S_IRWXG       00070
#define S_IRGRP       00040
#define S_IWGRP       00020
#define S_IXGRP       00010

#define S_IRWXO       00007
#define S_IROTH       00004
#define S_IWOTH       00002
#define S_IXOTH       00001

#define SEEK_SET      0
#define SEEK_CUR      1
#define SEEK_END      2

#define O_RDONLY      00000
#define O_WRONLY      00001
#define O_RDWR        00002
#define O_ACCMODE     00003
#define O_CREAT       01000
#define O_TRUNC       02000
#define O_EXCL        04000
#define O_NOCTTY      010000

#endif