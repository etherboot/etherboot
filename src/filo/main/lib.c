
#include <etherboot.h>
#include <lib.h>
#if 0
size_t strnlen(const char *src, size_t max)
{
    size_t i = 0;

    while ((*src++) && (i < max))
	i++;
    return i;
}
#endif

#if 0
size_t strlen(const char *src)
{
    size_t i = 0;

    while (*src++)
	i++;
    return i;
}
#endif
int strcmp(const char *s1, const char *s2)
{
    int r;

    while ((r = (*s1 - *s2)) == 0 && *s1) {
	s1++;
	s2++;
    }
    return r;
}
#if 0
int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t i;
    int retval;

    for (i = 0; i < n; i++) {
	retval = ((unsigned char *) s1)[i] - ((unsigned char *) s2)[i];
	if (retval)
	    return retval;
    }
    return 0;
}
#endif
#if 0
void *memcpy(void *dest, const void *src, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
	((char *) dest)[i] = ((char *) src)[i];
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
	((char *) s)[i] = c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    size_t i;

    if (d < s) {
	for (i = 0; i < n; i++)
	    d[i] = s[i];
    } else {
	for (i = n-1; i != (size_t)(-1); i--)
	    d[i] = s[i];
    }
    return dest;
}
#endif
int isspace(int c)
{
    switch (c) {
    case ' ': case '\f': case '\n':
    case '\r': case '\t': case '\v':
	return 1;
    default:
	return 0;
    }
}
#if 0
int isdigit(int c)
{
    switch (c) {
    case '0'...'9':
	return 1;
    default:
	return 0;
    }
}
#endif
int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
	return c - 'A' + 'a';
    return c;
}

char *strdup(const char *s)
{
    size_t sz = strlen(s) + 1;
    char *d = allot(sz);
    memcpy(d, s, sz);
    return d;
}

char *strchr(const char *s, int c)
{
    for (; *s; s++) {
	if (*s == c)
	    return (char *) s;
    }
    return 0;
}

char *strncpy(char *to, const char *from, int count)
{
    register char *ret = to;

    while (count > 0) {
	count--;
	if ((*to++ = *from++) == '\0')
	    break;
    }

    while (count > 0) {
	count--;
	*to++ = '\0';
    }
    return ret;
}

char *strcpy(char *to, const char *from)
{
    return strncpy(to, from, strlen(from));
}

unsigned int get_le32(const unsigned char *p)
{
    return ((unsigned int) p[0] << 0)
	| ((unsigned int) p[1] << 8)
	| ((unsigned int) p[2] << 16)
	| ((unsigned int) p[3] << 24);
}

unsigned int get_le16(const unsigned char *p)
{
    return ((unsigned int) p[0] << 0)
	| ((unsigned int) p[1] << 8);
}
#if (DEBUG_ALL || DEBUG_ELFBOOT || DEBUG_ELFNOTE || DEBUG_LINUXBIOS || \
	DEBUG_MALLOC || DEBUG_MULTIBOOT || DEBUG_SEGMENT || DEBUG_SYS_INFO ||\
	DEBUG_TIMER || DEBUG_BLOCKDEV || DEBUG_PCI || DEBUG_LINUXLOAD ||\
	DEBUG_IDE || DEBUG_ELTORITO)

// It is needed by debug for filo
void hexdump(const void *p, unsigned int len)
{
	int i;
	const unsigned char *q = p;

	for (i = 0; i < len; i++) {
	    if (i%16==0)
		printf("%04x: ", i);
	    printf("%02x%c", q[i], i%16==15 ? '\n' : i%8==7 ? '-' : ' ');
	}
	if (i%16 != 0)
	    putchar('\n');
}
#endif
