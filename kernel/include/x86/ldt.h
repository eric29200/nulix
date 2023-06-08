#ifndef _LDT_H_
#define _LDT_H_

int sys_modify_ldt(int func, void *ptr, unsigned long bytecount);

#endif