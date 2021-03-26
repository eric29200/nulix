#ifndef _IO_H_
#define _IO_H_

void outb(unsigned short port, unsigned char value);
unsigned char inb(unsigned short port);
unsigned short inw(unsigned short port);

#endif
