int main(void)
{
  char *tty = "/dev/tty0";
  int ret;

  /* open tty 0 */
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (6), "b" ((int) tty));

  return 0;
}
