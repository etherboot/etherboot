#ifdef	PASSWD

/*
 * This is an implementation of the "MD5 Message Digest Algorithm" as
 * described in
 *   Bruce Schneier 'Applied Cryptography', pages 436-441, 2nd Edition, 1996,
 *     John Wiley & Sons, Inc., ISBN 0-471-11709-9.
 * The MD5 algorithm has been invented by Ron Rivest/"RSA Data Security, Inc.";
 * Bruce Scheier refers to RFC1321 as the original publication.
 *
 * This implementation is copyright 1997 by M. Gutschke.
 *
 * This implementation has been optimized for size rather than for speed;
 * there are a few assumptions that require a little-endian architecture.
 * The maximum input length is 4GB.
 */

#include "etherboot.h"

static unsigned long md5_buf[16];
static unsigned long ABCD[4]={0x67452301L,0xefcdab89L,0x98badcfeL,0x10325476L};
static unsigned long md5_len = 0;

static unsigned long F(unsigned long X,unsigned long Y,unsigned long Z)
{ return((X&Y)|((~X)&Z)); }

static unsigned long G(unsigned long X,unsigned long Y,unsigned long Z)
{ return((X&Z)|(Y&(~Z))); }

static unsigned long H(unsigned long X,unsigned long Y,unsigned long Z)
{ return(X^Y^Z); }

static unsigned long I(unsigned long X,unsigned long Y,unsigned long Z)
{ return(Y^(X|(~Z))); }

static inline unsigned long rotate(unsigned long i,int s)
{ return((i << s) | (i >> (32-s))); }

static unsigned long ff(unsigned long (*fnc)(unsigned long,unsigned long,
					     unsigned long),
			unsigned long a,unsigned long b,unsigned long c,
			unsigned long d,unsigned long M,int s,
			unsigned long t)
{ return(b+(rotate((a+fnc(b,c,d)+M+t),s))); }

static void md5_loop(void)
{
  static unsigned char shifts[4][4] = {
    {22,17,12,7},{20,14,9,5},{23,16,11,4},{21,15,10,6}};
  static unsigned char idx[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
    1, 6,11, 0, 5,10,15, 4, 9,14, 3, 8,13, 2, 7,12,
    5, 8,11,14, 1, 4, 7,10,13, 0, 3, 6, 9,12,15, 2,
    0, 7,14, 5,12, 3,10, 1, 8,15, 6,13, 4,11, 2, 9 };
  static unsigned long masks[64] = {
    0xd76aa478L,0xe8c7b756L,0x242070dbL,0xc1bdceeeL,
    0xf57c0fafL,0x4787c62aL,0xa8304613L,0xfd469501L,
    0x698098d8L,0x8b44f7afL,0xffff5bb1L,0x895cd7beL,
    0x6b901122L,0xfd987193L,0xa679438eL,0x49b40821L,
    0xf61e2562L,0xc040b340L,0x265e5a51L,0xe9b6c7aaL,
    0xd62f105dL,0x02441453L,0xd8a1e681L,0xe7d3fbc8L,
    0x21e1cde6L,0xc33707d6L,0xf4d50d87L,0x455a14edL,
    0xa9e3e905L,0xfcefa3f8L,0x676f02d9L,0x8d2a4c8aL,
    0xfffa3942L,0x8771f681L,0x6d9d6122L,0xfde5380cL,
    0xa4beea44L,0x4bdecfa9L,0xf6bb4b60L,0xbebfbc70L,
    0x289b7ec6L,0xeaa127faL,0xd4ef3085L,0x04881d05L,
    0xd9d4d039L,0xe6db99e5L,0x1fa27cf8L,0xc4ac5665L,
    0xf4292244L,0x432aff97L,0xab9423a7L,0xfc93a039L,
    0x655b59c3L,0x8f0ccc92L,0xffeff47dL,0x85845dd1L,
    0x6fa87e4fL,0xfe2ce6e0L,0xa3014314L,0x4e0811a1L,
    0xf7537e82L,0xbd3af235L,0x2ad7d2bbL,0xeb86d391L};

  static unsigned long (*fncs[4])(unsigned long,unsigned long,
				  unsigned long) = { F,G,H,I };
  int i,j,k,l;
  unsigned long abcd[4];

  memcpy(abcd, ABCD, 16);
  for (i = j = 0; j < 4; j++)    /* rounds FF..II    */
    for (k = 0; k < 4; k++)      /* 0..3             */
      for (l = 4; l--; i++)      /* a,b,c,d..b,c,d,a */
	abcd[(l+1)&3] = ff(fncs[j],
			   abcd[(l+1)&3],abcd[(l+2)&3],abcd[(l+3)&3],abcd[l],
			   md5_buf[idx[i]],shifts[j][l],masks[i]);
  for (i = 4; i--; )
    ABCD[i] += abcd[i];
  return;
}

void md5_put(unsigned int ch)
{
  /* this code assumes a little endian architecture! */
  ((unsigned char *)md5_buf)[md5_len%64] = ch;
  if (((++md5_len)%64) == 0)
    md5_loop();
  return;
}

void md5_done(unsigned char *buf)
{
  unsigned long len = md5_len;
  int           i;

  /* this code assumes a little endian architecture! */
  md5_put(0x80);
  while ((md5_len%64) != 56)
    md5_put(0);
  md5_put(len <<  3); md5_put(len >>  5);
  md5_put(len >> 13); md5_put(len >> 21);
  for (i = 4; i--; )
    md5_put(0);
  memcpy(buf, ABCD, 16);
  md5_len = 0;
  {static unsigned long init[4]={
    0x67452301L,0xefcdab89L,0x98badcfeL,0x10325476L};
  memcpy(ABCD, init, 16); }
  return;
}

#endif
