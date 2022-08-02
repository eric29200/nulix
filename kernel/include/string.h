#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strdup(const char *s);
char *strchr(const char *s, char c);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);

void memset(void *s, char v, size_t n);
void memsetw(void *s, uint16_t v, size_t n);
void memsetdw(void *s, uint32_t v, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memcpyw(void *dest, const void *src, size_t n);
void *memcpydw(void *dest, const void *src, size_t n);

int atoi(const char *s);

#endif
