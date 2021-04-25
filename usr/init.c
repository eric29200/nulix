int main(void)
{
  char *tty = "/dev/tty0";
  char *msg = "Hello";
  int fd, ret;

  /* open tty 0 */
  __asm__ __volatile__("int $0x80" : "=a" (fd) : "0" (6), "b" ((int) tty));

  if (fd >= 0) {
    /* write to tty0 */
    __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (4), "b" ((int) fd), "c" ((int) msg), "d" ((int) 5));

    /* close tty 0 */
    __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (7), "b" (fd));
  }

  return 0;
}
