static inline int syscall1(int n, int p1)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" ((int) p1));
  return ret;
}

static inline int syscall2(int n, int p1, int p2)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" ((int) p1), "c" ((int) p2));
  return ret;
}

static inline int syscall3(int n, int p1, int p2, int p3)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" ((int) p1), "c" ((int) p2), "d" ((int) p3));
  return ret;
}

int main(void)
{
  char *tty = "/dev/tty0";
  char *msg = "Hello";
  int fd, ret;

  /* open tty 0 */
  fd = syscall1(6, (int) tty);

  if (fd >= 0) {
    /* write to tty0 */
    syscall3(4, fd, (int) msg, 5);

    /* close tty 0 */
    syscall1(7, fd);
  }

  return 0;
}
