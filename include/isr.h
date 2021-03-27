#ifndef _ISR_H_
#define _ISR_H_

struct registers_t {
  unsigned int ds;
  unsigned int edi;
  unsigned int esi;
  unsigned int ebp;
  unsigned int esp;
  unsigned int ebx;
  unsigned int edx;
  unsigned int ecx;
  unsigned int eax;
  unsigned int int_no;
  unsigned int err_code;
  unsigned int eip;
  unsigned int cs;
  unsigned int eflags;
  unsigned int useresp;
  unsigned int ss;
};

#endif
