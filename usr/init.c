int main(void)
{
  __asm__ __volatile__("int $0x80");
  return 0;
}
