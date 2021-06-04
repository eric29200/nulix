#ifndef _IO_H_
#define _IO_H_

#include <stddef.h>

static inline void outb(uint16_t port, uint8_t value)
{
  __asm__ __volatile__("outb %1, %0" : : "dN" (port), "a" (value));
}

static inline uint8_t inb(uint16_t port)
{
  uint8_t ret;
  __asm__ __volatile__("inb %1, %0" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline void outw(uint16_t port, uint16_t value)
{
  __asm__ __volatile__("outw %1, %0" : : "dN" (port), "a" (value));
}

static inline uint16_t inw(uint16_t port)
{
  uint16_t ret;
  __asm__ __volatile__("inw %1, %0" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline void outl(uint16_t port, uint32_t value)
{
  __asm__ __volatile__("outl %%eax, %%dx" : : "dN" (port), "a" (value));
}

static inline uint32_t inl(uint16_t port)
{
  uint32_t ret;
  __asm__ __volatile__("inl %%dx, %%eax" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline void outsw(uint16_t port, const void *addr, size_t count)
{
  __asm__ __volatile__("rep outsw" : "+S" (addr), "+c" (count) : "d" (port));
}

static inline void insw(uint16_t port, void *addr, size_t count)
{
  __asm__ __volatile__("rep insw" : "+D" (addr), "+c" (count) : "d" (port) : "memory");
}

#endif
