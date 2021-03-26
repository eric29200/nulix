#include "io.h"

/*
 * Write a byte to an io port.
 */
void outb(unsigned short port, unsigned char value)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

/*
 * Read a byte from an io port.
 */
unsigned char inb(unsigned short port)
{
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

/*
 * Read a word from an io port.
 */
unsigned short inw(unsigned short port)
{
    unsigned short ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}
