#include "../drivers/screen.h"
#include "../x86/gdt.h"

int main(struct multiboot *mboot_ptr)
{
  /* init gdt */
  init_gdt();

  /* clear screen */
  screen_clear();

  /* print message */
  screen_puts("Hello world1\n");
  screen_puts("Hello world2\n");
  screen_puts("Hello world3\n");
  screen_puts("Hello world4\n");
  screen_puts("Hello world5\n");
  screen_puts("Hello world6\n");
  screen_puts("Hello world7\n");
  screen_puts("Hello world8\n");
  screen_puts("Hello world9\n");
  screen_puts("Hello world10\n");
  screen_puts("Hello world11\n");
  screen_puts("Hello world12\n");
  screen_puts("Hello world13\n");
  screen_puts("Hello world14\n");
  screen_puts("Hello world15\n");
  screen_puts("Hello world16\n");
  screen_puts("Hello world17\n");
  screen_puts("Hello world18\n");
  screen_puts("Hello world19\n");
  screen_puts("Hello world20\n");
  screen_puts("Hello world21\n");
  screen_puts("Hello world22\n");
  screen_puts("Hello world23\n");
  screen_puts("Hello world24\n");
  screen_puts("Hello world25\n");
  screen_puts("Hello world26\n");

  return 0;
}
