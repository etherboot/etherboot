#ifndef ETHERBOOT_STRING_H
#define ETHERBOOT_STRING_H

extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern size_t strlen(const char *s);

#include "bits/string.h"

#endif /* ETHERBOOT_STRING */
