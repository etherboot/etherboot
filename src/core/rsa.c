#ifdef	SAFEBOOT
/*
	RSA.C - RSA routines for RSAEURO

    Copyright (c) J.S.A.Kapp 1994 - 1996.

	RSAEURO - RSA Library compatible with RSAREF(tm) 2.0.

	All functions prototypes are the Same as for RSAREF(tm).
	To aid compatiblity the source and the files follow the
	same naming comventions that RSAREF(tm) uses.  This should aid
	direct importing to your applications.

	This library is legal everywhere outside the US.  And should
	NOT be imported to the US and used there.
		Note: This seems to be outdated in 2003

	All Trademarks Acknowledged.

	RSA encryption performed as defined in the PKCS (#1) by RSADSI.
		Here: No encryption, decryption only

	Revision history
		0.90 First revision, code produced very similar to that
		of RSAREF(tm), still it worked fine.

        0.91 Second revision, code altered to aid speeding up.
		Used pointer accesses to arrays to speed up some parts,
		mainly during the loops.

        1.03 Third revision, Random Structure initialization
        double check, RSAPublicEncrypt can now return RE_NEED_RANDOM.

	This file has been modified to be used with Etherboot by
	Anselm Martin Hoffmeister (anselm at hoffmeister-online dot de)
	This happened after permission by RSAEuro.
	Changed: Moved everything needed for RSA-decrypt into a single file
	and removed lots of code that's not needed for etherboot
	Changes (C) April, May 2003 AMH
*/

//#include <string.h>
/*****************************************************************************
 ****************************************************** #include "global.h" */
#ifndef PROTOTYPES
#define PROTOTYPES 1
#endif

typedef unsigned char *POINTER; /* POINTER defines a generic pointer type */
typedef unsigned short int UINT2;/* UINT2 defines a two byte word */
typedef unsigned long int UINT4;/* UINT4 defines a four byte word */
typedef unsigned char BYTE;/* BYTE defines a unsigned character */
typedef signed long int signeddigit;/* internal signed value */
#ifndef NULL_PTR
#define NULL_PTR ((POINTER)0)
#endif
#ifndef UNUSED_ARG
#define UNUSED_ARG(x) x = *(&x);
#endif
/* PROTO_LIST is defined depending on how PROTOTYPES is defined above.
	 If using PROTOTYPES, then PROTO_LIST returns the list, otherwise it
	 returns an empty list. */
#if PROTOTYPES
#define PROTO_LIST(list) list
#else
#define PROTO_LIST(list) ()
#endif
/*****************************************************************************
 ********************************************************** #include "nn.h" */
/* Type definitions. */
typedef UINT4 NN_DIGIT;
typedef UINT2 NN_HALF_DIGIT;
/* Constants.
	 Note: MAX_NN_DIGITS is long enough to hold any RSA modulus, plus
   one more digit as required by R_GeneratePEMKeys (for n and phiN,
   whose lengths must be even). All natural numbers have at most
   MAX_NN_DIGITS digits, except for double-length intermediate values
	 in NN_Mult (t), NN_ModMult (t), NN_ModInv (w), and NN_Div (c).
*/
#define NN_DIGIT_BITS 32 /* Length of digit in bits */
#define NN_HALF_DIGIT_BITS 16 /* Length of digit in bytes */
#define NN_DIGIT_LEN (NN_DIGIT_BITS / 8) /* Maximum length in digits */
#define MAX_NN_DIGITS \
  ((MAX_RSA_MODULUS_LEN + NN_DIGIT_LEN - 1) / NN_DIGIT_LEN + 1)
#define MAX_NN_DIGIT 0xffffffff /* Maximum digits */
#define MAX_NN_HALF_DIGIT 0xffff
#define NN_LT   -1
#define NN_EQ   0
#define NN_GT 1
#define LOW_HALF(x) ((x) & MAX_NN_HALF_DIGIT)
#define HIGH_HALF(x) (((x) >> NN_HALF_DIGIT_BITS) & MAX_NN_HALF_DIGIT)
#define TO_HIGH_HALF(x) (((NN_DIGIT)(x)) << NN_HALF_DIGIT_BITS)
#define DIGIT_MSB(x) (unsigned int)(((x) >> (NN_DIGIT_BITS - 1)) & 1)
#define DIGIT_2MSB(x) (unsigned int)(((x) >> (NN_DIGIT_BITS - 2)) & 3)
/* CONVERSIONS
   NN_Decode (a, digits, b, len)   Decodes character string b into a.
   NN_Encode (a, len, b, digits)   Encodes a into character string b.

   ASSIGNMENTS
   NN_Assign (a, b, digits)        Assigns a = b.
   NN_ASSIGN_DIGIT (a, b, digits)  Assigns a = b, where b is a digit.
	 NN_AssignZero (a, b, digits)    Assigns a = 0.
   NN_Assign2Exp (a, b, digits)    Assigns a = 2^b.
     
   ARITHMETIC OPERATIONS
	 NN_Add (a, b, c, digits)        Computes a = b + c.
   NN_Sub (a, b, c, digits)        Computes a = b - c.
   NN_Mult (a, b, c, digits)       Computes a = b * c.
   NN_LShift (a, b, c, digits)     Computes a = b * 2^c.
   NN_RShift (a, b, c, digits)     Computes a = b / 2^c.
   NN_Div (a, b, c, cDigits, d, dDigits)  Computes a = c div d and b = c mod d.

   NUMBER THEORY
   NN_Mod (a, b, bDigits, c, cDigits)  Computes a = b mod c.
   NN_ModMult (a, b, c, d, digits) Computes a = b * c mod d.
   NN_ModExp (a, b, c, cDigits, d, dDigits)  Computes a = b^c mod d.
   NN_ModInv (a, b, c, digits)     Computes a = 1/b mod c.
   NN_Gcd (a, b, c, digits)        Computes a = gcd (b, c).

   OTHER OPERATIONS
   NN_EVEN (a, digits)             Returns 1 iff a is even.
   NN_Cmp (a, b, digits)           Returns sign of a - b.
   NN_EQUAL (a, digits)            Returns 1 iff a = b.
   NN_Zero (a, digits)             Returns 1 iff a = 0.
	 NN_Digits (a, digits)           Returns significant length of a in digits.
   NN_Bits (a, digits)             Returns significant length of a in bits.
 */
void NN_Decode PROTO_LIST
  ((NN_DIGIT *, unsigned int, unsigned char *, unsigned int));
void NN_Encode PROTO_LIST
  ((unsigned char *, unsigned int, NN_DIGIT *, unsigned int));

void NN_Assign PROTO_LIST ((NN_DIGIT *, NN_DIGIT *, unsigned int));
void NN_AssignZero PROTO_LIST ((NN_DIGIT *, unsigned int));
void NN_Assign2Exp PROTO_LIST ((NN_DIGIT *, unsigned int, unsigned int));

NN_DIGIT NN_Add PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));
NN_DIGIT NN_Sub PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));
void NN_Mult PROTO_LIST ((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));
void NN_Div PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int, NN_DIGIT *,
		unsigned int));
NN_DIGIT NN_LShift PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, unsigned int, unsigned int));
NN_DIGIT NN_RShift PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, unsigned int, unsigned int));
NN_DIGIT NN_LRotate PROTO_LIST 
	((NN_DIGIT *, NN_DIGIT *, unsigned int, unsigned int));

void NN_Mod PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, unsigned int, NN_DIGIT *, unsigned int));
void NN_ModMult PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));
void NN_ModExp PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int, NN_DIGIT *,
		unsigned int));
void NN_ModInv PROTO_LIST
	((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));
void NN_Gcd PROTO_LIST ((NN_DIGIT *, NN_DIGIT *, NN_DIGIT *, unsigned int));

int NN_Cmp PROTO_LIST ((NN_DIGIT *, NN_DIGIT *, unsigned int));
int NN_Zero PROTO_LIST ((NN_DIGIT *, unsigned int));
unsigned int NN_Bits PROTO_LIST ((NN_DIGIT *, unsigned int));
unsigned int NN_Digits PROTO_LIST ((NN_DIGIT *, unsigned int));
#define NN_ASSIGN_DIGIT(a, b, digits) {NN_AssignZero (a, digits); a[0] = b;}
#define NN_EQUAL(a, b, digits) (! NN_Cmp (a, b, digits))
#define NN_EVEN(a, digits) (((digits) == 0) || ! (a[0] & 1))
/****************************************************************************
 ********************************************************* #include "md2.h" */
typedef struct {
  unsigned char state[16];                                 /* state */
  unsigned char checksum[16];                           /* checksum */
  unsigned int count;                 /* number of bytes, modulo 16 */
  unsigned char buffer[16];                         /* input buffer */
} MD2_CTX;
void MD2Init PROTO_LIST ((MD2_CTX *));
void MD2Update PROTO_LIST
  ((MD2_CTX *, unsigned char *, unsigned int));
void MD2Final PROTO_LIST ((unsigned char [16], MD2_CTX *));
/****************************************************************************
 ********************************************************* #include "md4.h" */
typedef struct {
	UINT4 state[4];                                   /* state (ABCD) */
	UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                         /* input buffer */
} MD4_CTX;
void MD4Init PROTO_LIST ((MD4_CTX *));
void MD4Update PROTO_LIST ((MD4_CTX *, unsigned char *, unsigned int));
void MD4Final PROTO_LIST ((unsigned char [16], MD4_CTX *));
/****************************************************************************
 ********************************************************* #include "md5.h" */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;
void MD5Init PROTO_LIST ((MD5_CTX *));
void MD5Update PROTO_LIST
  ((MD5_CTX *, unsigned char *, unsigned int));
void MD5Final PROTO_LIST ((unsigned char [16], MD5_CTX *));
/****************************************************************************
 ********************************************************* #include "shs.h" */
/* The SHS block size and message digest sizes, in bytes */
#define SHS_BLOCKSIZE   64
#define SHS_DIGESTSIZE  20
/* The structure for storing SHS info */
typedef struct {
	UINT4 digest [5];             /* Message digest */
	UINT4 countLo, countHi;       /* 64-bit bit count */
	UINT4 data [16];              /* SHS data buffer */
} SHS_CTX;
void SHSInit PROTO_LIST ((SHS_CTX *));
void SHSUpdate PROTO_LIST ((SHS_CTX *, unsigned char *, int ));
void SHSFinal PROTO_LIST ((unsigned char *, SHS_CTX *));
/****************************************************************************
 ********************************************************* #include "des.h" */
typedef struct {
  UINT4 subkeys[32];                                             /* subkeys */
  UINT4 iv[2];                                       /* initializing vector */
  UINT4 originalIV[2];                        /* for restarting the context */
  int encrypt;                                               /* encrypt flag */
} DES_CBC_CTX;
typedef struct {
  UINT4 subkeys[32];                                             /* subkeys */
  UINT4 iv[2];                                       /* initializing vector */
  UINT4 inputWhitener[2];                                 /* input whitener */
  UINT4 outputWhitener[2];                               /* output whitener */
  UINT4 originalIV[2];                        /* for restarting the context */
  int encrypt;                                              /* encrypt flag */
} DESX_CBC_CTX;
typedef struct {
  UINT4 subkeys[3][32];                     /* subkeys for three operations */
  UINT4 iv[2];                                       /* initializing vector */
  UINT4 originalIV[2];                        /* for restarting the context */
  int encrypt;                                              /* encrypt flag */
} DES3_CBC_CTX;
void DES_CBCInit PROTO_LIST 
  ((DES_CBC_CTX *, unsigned char *, unsigned char *, int));
int DES_CBCUpdate PROTO_LIST
  ((DES_CBC_CTX *, unsigned char *, unsigned char *, unsigned int));
void DES_CBCRestart PROTO_LIST ((DES_CBC_CTX *));
void DESX_CBCInit PROTO_LIST
  ((DESX_CBC_CTX *, unsigned char *, unsigned char *, int));
int DESX_CBCUpdate PROTO_LIST
  ((DESX_CBC_CTX *, unsigned char *, unsigned char *, unsigned int));
void DESX_CBCRestart PROTO_LIST ((DESX_CBC_CTX *));
void DES3_CBCInit PROTO_LIST 
  ((DES3_CBC_CTX *, unsigned char *, unsigned char *, int));
int DES3_CBCUpdate PROTO_LIST
  ((DES3_CBC_CTX *, unsigned char *, unsigned char *, unsigned int));
void DES3_CBCRestart PROTO_LIST ((DES3_CBC_CTX *));
/*****************************************************************************
 ***************************************************** #include "rsaeuro.h" */
/* Message-digest algorithms. */
#define DA_MD2 2
#define DA_MD4 4
#define DA_MD5 5
#define DA_SHS 3
/* Encryption algorithms to be ored with digest algorithm in Seal and Open. */
#define EA_DES_CBC 1
#define EA_DES_EDE2_CBC 2
#define EA_DES_EDE3_CBC 3
#define EA_DESX_CBC 4
/* RSA key lengths. */
#define MIN_RSA_MODULUS_BITS 508
#define MAX_RSA_MODULUS_BITS 1024
#define MAX_RSA_MODULUS_LEN ((MAX_RSA_MODULUS_BITS + 7) / 8)
#define MAX_RSA_PRIME_BITS ((MAX_RSA_MODULUS_BITS + 1) / 2)
#define MAX_RSA_PRIME_LEN ((MAX_RSA_PRIME_BITS + 7) / 8)
/* Maximum lengths of encoded and encrypted content, as a function of
	 content length len. Also, inverse functions. */
#define ENCODED_CONTENT_LEN(len) (4*(len)/3 + 3)
#define ENCRYPTED_CONTENT_LEN(len) ENCODED_CONTENT_LEN ((len)+8)
#define DECODED_CONTENT_LEN(len) (3*(len)/4 + 1)
#define DECRYPTED_CONTENT_LEN(len) (DECODED_CONTENT_LEN (len) - 1)
/* Maximum lengths of signatures, encrypted keys, encrypted
	 signatures, and message digests. */
#define MAX_SIGNATURE_LEN MAX_RSA_MODULUS_LEN
#define MAX_PEM_SIGNATURE_LEN ENCODED_CONTENT_LEN(MAX_SIGNATURE_LEN)
#define MAX_ENCRYPTED_KEY_LEN MAX_RSA_MODULUS_LEN
#define MAX_PEM_ENCRYPTED_KEY_LEN ENCODED_CONTENT_LEN(MAX_ENCRYPTED_KEY_LEN)
#define MAX_PEM_ENCRYPTED_SIGNATURE_LEN ENCRYPTED_CONTENT_LEN(MAX_SIGNATURE_LEN)
#define MAX_DIGEST_LEN 20
/* Maximum length of Diffie-Hellman parameters. */
#define DH_PRIME_LEN(bits) (((bits) + 7) / 8)
/* Error codes. */
#define RE_CONTENT_ENCODING 0x0400
#define RE_DATA 0x0401
#define RE_DIGEST_ALGORITHM 0x0402
#define RE_ENCODING 0x0403
#define RE_KEY 0x0404
#define RE_KEY_ENCODING 0x0405
#define RE_LEN 0x0406
#define RE_MODULUS_LEN 0x0407
#define RE_NEED_RANDOM 0x0408
#define RE_PRIVATE_KEY 0x0409
#define RE_PUBLIC_KEY 0x040a
#define RE_SIGNATURE 0x040b
#define RE_SIGNATURE_ENCODING 0x040c
#define RE_ENCRYPTION_ALGORITHM 0x040d
#define RE_FILE 0x040e
/* Library details. */
#define RSAEURO_VER_MAJ 1
#define RSAEURO_VER_MIN 04
#define RSAEURO_IDENT "RSAEURO Toolkit"
#define RSAEURO_DATE "21/08/94"
/* Internal Error Codes */
/* IDOK and IDERROR changed to ID_OK and ID_ERROR */
#define ID_OK    0
#define ID_ERROR 1
/* Internal defs. */
#define TRUE    1
#define FALSE   0
/* Algorithm IDs */
#define IA_MD2 0x00000001
#define IA_MD4 0x00000002
#define IA_MD5 0x00000004
#define IA_SHS 0x00000008
#define IA_DES_CBC 0x00000010
#define IA_DES_EDE2_CBC 0x00000020
#define IA_DES_EDE3_CBC 0x00000040
#define IA_DESX_CBC 0x00000080
#define IA_RSA 0x00010000
#define IA_DH  0x00020000
#define IA_FLAGS (IA_MD2|IA_MD4|IA_MD5|IA_SHS|IA_DES_CBC|IA_DES_EDE2_CBC|IA_DES_EDE3_CBC|IA_DESX_CBC|IA_RSA|IA_DH)
/* RSAEuro Info Structure */
typedef struct {
    unsigned short int Version;                 /* RSAEuro Version */
    unsigned int flags;                         /* Version Flags */
    unsigned char ManufacturerID[32];           /* Toolkit ID */
    unsigned int Algorithms;                    /* Algorithms Supported */
} RSAEUROINFO;
/* Random structure. */
typedef struct {
  unsigned int bytesNeeded;                    /* seed bytes required */
  unsigned char state[16];                     /* state of object */
  unsigned int outputAvailable;                /* number byte available */
  unsigned char output[16];                    /* output bytes */
} R_RANDOM_STRUCT;
/* RSA public and private key. */
typedef struct {
  unsigned short int bits;                     /* length in bits of modulus */
  unsigned char modulus[MAX_RSA_MODULUS_LEN];  /* modulus */
  unsigned char exponent[MAX_RSA_MODULUS_LEN]; /* public exponent */
} R_RSA_PUBLIC_KEY;
typedef struct {
  unsigned short int bits;                     /* length in bits of modulus */
  unsigned char modulus[MAX_RSA_MODULUS_LEN];  /* modulus */
  unsigned char publicExponent[MAX_RSA_MODULUS_LEN];     /* public exponent */
  unsigned char exponent[MAX_RSA_MODULUS_LEN]; /* private exponent */
  unsigned char prime[2][MAX_RSA_PRIME_LEN];   /* prime factors */
  unsigned char primeExponent[2][MAX_RSA_PRIME_LEN];     /* exponents for CRT */
  unsigned char coefficient[MAX_RSA_PRIME_LEN];          /* CRT coefficient */
} R_RSA_PRIVATE_KEY;
/* RSA prototype key. */
typedef struct {
  unsigned int bits;                           /* length in bits of modulus */
  int useFermat4;                              /* public exponent (1 = F4, 0 = 3) */
} R_RSA_PROTO_KEY;
/* Diffie-Hellman parameters. */
typedef struct {
  unsigned char *prime;                        /* prime */
  unsigned int primeLen;                       /* length of prime */
  unsigned char *generator;                    /* generator */
  unsigned int generatorLen;                   /* length of generator */
} R_DH_PARAMS;
/* digest algorithm context */
typedef struct {
  int digestAlgorithm;                         /* digest type */
  union {                                      /* digest sub-context */
		MD2_CTX md2;
		MD4_CTX md4;
		MD5_CTX md5;
		SHS_CTX shs;
	} context;
} R_DIGEST_CTX;
/* signature context */
typedef struct {
	R_DIGEST_CTX digestContext;
} R_SIGNATURE_CTX;
/* envelope context */
typedef struct {
  int encryptionAlgorithm;                       /* encryption type */
  union {                                      /* encryption sub-context */
		DES_CBC_CTX des;
		DES3_CBC_CTX des3;
		DESX_CBC_CTX desx;
  } cipherContext;

  unsigned char buffer[8];                       /* data buffer */
  unsigned int bufferLen;                      /* buffer length */
} R_ENVELOPE_CTX;
/* Cryptographic procedures. */
int R_OpenInit PROTO_LIST ((R_ENVELOPE_CTX *, int, unsigned char *,
	unsigned int, unsigned char [8], R_RSA_PRIVATE_KEY *));
int R_OpenUpdate PROTO_LIST ((R_ENVELOPE_CTX *, unsigned char *,
	unsigned int *, unsigned char *, unsigned int));
int R_OpenFinal PROTO_LIST ((R_ENVELOPE_CTX *, unsigned char *,
	unsigned int *));
/* Cryptographic enhancements. */
int R_SignBlock PROTO_LIST ((unsigned char *, unsigned int *, unsigned char *, unsigned int, int,
	R_RSA_PRIVATE_KEY *));
int R_VerifyPEMSignature PROTO_LIST ((unsigned char *, unsigned int *, unsigned char *, unsigned int,
	unsigned char *, unsigned int, int, int, R_RSA_PUBLIC_KEY *));
int R_VerifyBlockSignature PROTO_LIST ((unsigned char *, unsigned int,
	unsigned char *, unsigned int, int, R_RSA_PUBLIC_KEY *));
/* Standard library routines. */
#ifdef USE_ANSI
#define R_memset(x, y, z) memset(x, y, z)
#define R_memcpy(x, y, z) memcpy(x, y, z)
#define R_memcmp(x, y, z) memcmp(x, y, z)
#else
void R_memset PROTO_LIST ((POINTER, int, unsigned int));
void R_memcpy PROTO_LIST ((POINTER, POINTER, unsigned int));
int R_memcmp PROTO_LIST ((POINTER, POINTER, unsigned int));
#endif
/*****************************************************************************
 ********************************************************* #include "rsa.h" */
int RSAPublicDecrypt PROTO_LIST ((unsigned char *, unsigned int *, unsigned char *, unsigned int,
    R_RSA_PUBLIC_KEY *));
/*****************************************************************************
 ******************************************************************* nn.c ***/
/* internal static functions */
static NN_DIGIT subdigitmult PROTO_LIST ((NN_DIGIT *, NN_DIGIT *, NN_DIGIT, NN_DIGIT *, unsigned int));
static void dmult PROTO_LIST ((NN_DIGIT, NN_DIGIT, NN_DIGIT *, NN_DIGIT *));
static unsigned int NN_DigitBits PROTO_LIST ((NN_DIGIT));
#ifndef USEASM
/* Decodes character string b into a, where character string is ordered
	 from most to least significant.
	 Lengths: a[digits], b[len]. Assumes b[i] = 0 for i < len - digits * NN_DIGIT_LEN. (Otherwise most
	 significant bytes are truncated.)
 */
void NN_Decode (NN_DIGIT *a, unsigned int digits, unsigned char * b, unsigned int len)
{
  NN_DIGIT t;  unsigned int i, u;  int j;
  for (i = 0, j = len - 1; i < digits && j >= 0; i++) {    t = 0;
    for (u = 0; j >= 0 && u < NN_DIGIT_BITS; j--, u += 8)
			t |= ((NN_DIGIT)b[j]) << u;
		a[i] = t;
  }
  for (; i < digits; i++)    a[i] = 0;
}
/* Encodes b into character string a, where character string is ordered
   from most to least significant.
	 Lengths: a[len], b[digits].
	 Assumes NN_Bits (b, digits) <= 8 * len. (Otherwise most significant
	 digits are truncated.)
 */
void NN_Encode (unsigned char *a, unsigned int len, NN_DIGIT *b, unsigned int digits)
{
	NN_DIGIT t;    unsigned int i, u;    int j;
    for (i = 0, j = len - 1; i < digits && j >= 0; i++) {		t = b[i];
        for (u = 0; j >= 0 && u < NN_DIGIT_BITS; j--, u += 8)
			a[j] = (unsigned char)(t >> u);
	}
    for (; j >= 0; j--)	a[j] = 0;
}
void NN_AssignZero (NN_DIGIT *a, unsigned int digits) {if(digits) {do {*a++ = 0;}while(--digits);}}
#endif

/* Assigns a = 2^b.
   Lengths: a[digits].
	 Requires b < digits * NN_DIGIT_BITS.
 */
void NN_Assign2Exp (NN_DIGIT *a, unsigned int b, unsigned int digits)
{  NN_AssignZero (a, digits);	if (b >= digits * NN_DIGIT_BITS)    return;
  a[b / NN_DIGIT_BITS] = (NN_DIGIT)1 << (b % NN_DIGIT_BITS);
}
/* Computes a = b - c. Returns borrow.
	 Lengths: a[digits], b[digits], c[digits].
 */
NN_DIGIT NN_Sub (NN_DIGIT *a, NN_DIGIT *b, NN_DIGIT *c, unsigned int digits)
{
	NN_DIGIT temp, borrow = 0;
	if(digits)
		do { temp = *b - borrow;  b++;
                 if(temp == MAX_NN_DIGIT) {
                  temp = MAX_NN_DIGIT - *c;
                  c++;
                 }else {      /* Patch to prevent bug for Sun CC */
                  if((temp -= *c) > (MAX_NN_DIGIT - *c)) borrow = 1;	else	borrow = 0;
                  c++;
                 }
		 *a++ = temp;
		}while(--digits);
	return(borrow);
}
/* Computes a = b * c.
	 Lengths: a[2*digits], b[digits], c[digits].
	 Assumes digits < MAX_NN_DIGITS.
*/

void NN_Mult (NN_DIGIT *a, NN_DIGIT *b, NN_DIGIT *c, unsigned int digits)
{
	NN_DIGIT t[2*MAX_NN_DIGITS];
	NN_DIGIT dhigh, dlow, carry;
	unsigned int bDigits, cDigits, i, j;
	NN_AssignZero (t, 2 * digits);
	bDigits = NN_Digits (b, digits);
	cDigits = NN_Digits (c, digits);
	for (i = 0; i < bDigits; i++) {
		carry = 0;
		if(*(b+i) != 0) {
			for(j = 0; j < cDigits; j++) {
				dmult(*(b+i), *(c+j), &dhigh, &dlow);
				if((*(t+(i+j)) = *(t+(i+j)) + carry) < carry)	carry = 1;	else	carry = 0;
				if((*(t+(i+j)) += dlow) < dlow)
					carry++;
				carry += dhigh;
			}
		}
		*(t+(i+cDigits)) += carry;
	}
	NN_Assign(a, t, 2 * digits);
}
/* Computes a = b * 2^c (i.e., shifts left c bits), returning carry.
	 Requires c < NN_DIGIT_BITS. */
NN_DIGIT NN_LShift (NN_DIGIT * a, NN_DIGIT * b, unsigned int c, unsigned int digits)
{
	NN_DIGIT temp, carry = 0;  unsigned int t;
	if(c < NN_DIGIT_BITS)
		if(digits) {
			t = NN_DIGIT_BITS - c;
			do {
				temp = *b++;
				*a++ = (temp << c) | carry;
				carry = c ? (temp >> t) : 0;
			}while(--digits);
		}
	return (carry);
}
/* Computes a = c div 2^c (i.e., shifts right c bits), returning carry.
	 Requires: c < NN_DIGIT_BITS. */
NN_DIGIT NN_RShift (NN_DIGIT *a, NN_DIGIT *b, unsigned int c, unsigned int digits)
{
	NN_DIGIT temp, carry = 0; unsigned int t;
	if(c < NN_DIGIT_BITS)
		if(digits) {
			t = NN_DIGIT_BITS - c;
			do {
				digits--;
				temp = *(b+digits);
				*(a+digits) = (temp >> c) | carry;
				carry = c ? (temp << t) : 0;
			}while(digits);
		}
	return (carry);
}
/* Computes a = c div d and b = c mod d.
	 Lengths: a[cDigits], b[dDigits], c[cDigits], d[dDigits].
	 Assumes d > 0, cDigits < 2 * MAX_NN_DIGITS,
					 dDigits < MAX_NN_DIGITS.
*/
void NN_Div (a, b, c, cDigits, d, dDigits)
NN_DIGIT *a, *b, *c, *d; unsigned int cDigits, dDigits;
{
	NN_DIGIT ai, cc[2*MAX_NN_DIGITS+1], dd[MAX_NN_DIGITS], s;
	NN_DIGIT t[2], u, v, *ccptr;
	NN_HALF_DIGIT aHigh, aLow, cHigh, cLow;
	int i;	unsigned int ddDigits, shift;
	ddDigits = NN_Digits (d, dDigits);
	if(ddDigits == 0)
		return;
	shift = NN_DIGIT_BITS - NN_DigitBits (d[ddDigits-1]);
	NN_AssignZero (cc, ddDigits);
	cc[cDigits] = NN_LShift (cc, c, shift, cDigits);
	NN_LShift (dd, d, shift, ddDigits);
	s = dd[ddDigits-1];
	NN_AssignZero (a, cDigits);
	for (i = cDigits-ddDigits; i >= 0; i--) {
		if (s == MAX_NN_DIGIT)
			ai = cc[i+ddDigits];
		else {
			ccptr = &cc[i+ddDigits-1];
			s++;
			cHigh = (NN_HALF_DIGIT)HIGH_HALF (s);
			cLow = (NN_HALF_DIGIT)LOW_HALF (s);
			*t = *ccptr;
			*(t+1) = *(ccptr+1);
			if (cHigh == MAX_NN_HALF_DIGIT)
				aHigh = (NN_HALF_DIGIT)HIGH_HALF (*(t+1));
			else
				aHigh = (NN_HALF_DIGIT)(*(t+1) / (cHigh + 1));
			u = (NN_DIGIT)aHigh * (NN_DIGIT)cLow;
			v = (NN_DIGIT)aHigh * (NN_DIGIT)cHigh;
			if ((*t -= TO_HIGH_HALF (u)) > (MAX_NN_DIGIT - TO_HIGH_HALF (u)))
				t[1]--;
			*(t+1) -= HIGH_HALF (u);
			*(t+1) -= v;
			while ((*(t+1) > cHigh) ||
						 ((*(t+1) == cHigh) && (*t >= TO_HIGH_HALF (cLow)))) {
				if ((*t -= TO_HIGH_HALF (cLow)) > MAX_NN_DIGIT - TO_HIGH_HALF (cLow))
					t[1]--;
				*(t+1) -= cHigh;
				aHigh++;
			}
			if (cHigh == MAX_NN_HALF_DIGIT)
				aLow = (NN_HALF_DIGIT)LOW_HALF (*(t+1));
			else
				aLow =
			(NN_HALF_DIGIT)((TO_HIGH_HALF (*(t+1)) + HIGH_HALF (*t)) / (cHigh + 1));
			u = (NN_DIGIT)aLow * (NN_DIGIT)cLow;
			v = (NN_DIGIT)aLow * (NN_DIGIT)cHigh;
			if ((*t -= u) > (MAX_NN_DIGIT - u))
				t[1]--;
			if ((*t -= TO_HIGH_HALF (v)) > (MAX_NN_DIGIT - TO_HIGH_HALF (v)))
				t[1]--;
			*(t+1) -= HIGH_HALF (v);
			while ((*(t+1) > 0) || ((*(t+1) == 0) && *t >= s)) {
				if ((*t -= s) > (MAX_NN_DIGIT - s))
					t[1]--;
				aLow++;
			}
			ai = TO_HIGH_HALF (aHigh) + aLow;
			s--;
		}
		cc[i+ddDigits] -= subdigitmult(&cc[i], &cc[i], ai, dd, ddDigits);
		while (cc[i+ddDigits] || (NN_Cmp (&cc[i], dd, ddDigits) >= 0)) {
			ai++;
			cc[i+ddDigits] -= NN_Sub (&cc[i], &cc[i], dd, ddDigits);
		}
		a[i] = ai;
	}
	NN_AssignZero (b, dDigits);
	NN_RShift (b, cc, shift, ddDigits);
}
/* Computes a = b mod c.
	 Lengths: a[cDigits], b[bDigits], c[cDigits].
	 Assumes c > 0, bDigits < 2 * MAX_NN_DIGITS, cDigits < MAX_NN_DIGITS.
*/
void NN_Mod (a, b, bDigits, c, cDigits)
NN_DIGIT *a, *b, *c; unsigned int bDigits, cDigits;
{    NN_DIGIT t[2 * MAX_NN_DIGITS];	NN_Div (t, a, b, bDigits, c, cDigits);  }
/* Computes a = b * c mod d.
   Lengths: a[digits], b[digits], c[digits], d[digits].
   Assumes d > 0, digits < MAX_NN_DIGITS.
 */
void NN_ModMult (a, b, c, d, digits)
NN_DIGIT *a, *b, *c, *d; unsigned int digits;
{    NN_DIGIT t[2*MAX_NN_DIGITS]; NN_Mult (t, b, c, digits); NN_Mod (a, t, 2 * digits, d, digits); }
/* Computes a = b^c mod d.
   Lengths: a[dDigits], b[dDigits], c[cDigits], d[dDigits].
	 Assumes d > 0, cDigits > 0, dDigits < MAX_NN_DIGITS.
 */
void NN_ModExp (a, b, c, cDigits, d, dDigits)
NN_DIGIT *a, *b, *c, *d; unsigned int cDigits, dDigits;
{
    NN_DIGIT bPower[3][MAX_NN_DIGITS], ci, t[MAX_NN_DIGITS];
    int i;
	unsigned int ciBits, j, s;
	/* Store b, b^2 mod d, and b^3 mod d.
	 */
	NN_Assign (bPower[0], b, dDigits);
	NN_ModMult (bPower[1], bPower[0], b, d, dDigits);
    NN_ModMult (bPower[2], bPower[1], b, d, dDigits);
    NN_ASSIGN_DIGIT (t, 1, dDigits);
	cDigits = NN_Digits (c, cDigits);
    for (i = cDigits - 1; i >= 0; i--) {
		ci = c[i];
		ciBits = NN_DIGIT_BITS;
		/* Scan past leading zero bits of most significant digit.
		 */
		if (i == (int)(cDigits - 1)) {
			while (! DIGIT_2MSB (ci)) {
				ci <<= 2;
				ciBits -= 2;
			}
        }
        for (j = 0; j < ciBits; j += 2, ci <<= 2) {
        /* Compute t = t^4 * b^s mod d, where s = two MSB's of ci. */
            NN_ModMult (t, t, t, d, dDigits);
            NN_ModMult (t, t, t, d, dDigits);
            if ((s = DIGIT_2MSB (ci)) != 0)
            NN_ModMult (t, t, bPower[s-1], d, dDigits);
        }
    }
	NN_Assign (a, t, dDigits);
}

/* Compute a = 1/b mod c, assuming inverse exists.
   Lengths: a[digits], b[digits], c[digits].
	 Assumes gcd (b, c) = 1, digits < MAX_NN_DIGITS.
 */
void NN_ModInv (a, b, c, digits)
NN_DIGIT *a, *b, *c;
unsigned int digits;
{
    NN_DIGIT q[MAX_NN_DIGITS], t1[MAX_NN_DIGITS], t3[MAX_NN_DIGITS],
		u1[MAX_NN_DIGITS], u3[MAX_NN_DIGITS], v1[MAX_NN_DIGITS],
		v3[MAX_NN_DIGITS], w[2*MAX_NN_DIGITS];
    int u1Sign;
    /* Apply extended Euclidean algorithm, modified to avoid negative
       numbers.
    */
    NN_ASSIGN_DIGIT (u1, 1, digits);
	NN_AssignZero (v1, digits);
    NN_Assign (u3, b, digits);
	NN_Assign (v3, c, digits);
    u1Sign = 1;
	while (! NN_Zero (v3, digits)) {
        NN_Div (q, t3, u3, digits, v3, digits);
        NN_Mult (w, q, v1, digits);
		NN_Add (t1, u1, w, digits);
        NN_Assign (u1, v1, digits);
		NN_Assign (v1, t1, digits);
		NN_Assign (u3, v3, digits);
		NN_Assign (v3, t3, digits);
		u1Sign = -u1Sign;
	}
    /* Negate result if sign is negative. */
	if (u1Sign < 0)	NN_Sub (a, c, u1, digits); else	NN_Assign (a, u1, digits);
}
/* Computes a = gcd(b, c).
	 Assumes b > c, digits < MAX_NN_DIGITS.
*/
#define iplus1  ( i==2 ? 0 : i+1 )      /* used by Euclid algorithms */
#define iminus1 ( i==0 ? 2 : i-1 )      /* used by Euclid algorithms */
#define g(i) (  &(t[i][0])  )
void NN_Gcd(a ,b ,c, digits) NN_DIGIT *a, *b, *c; unsigned int digits;
{
	short i; NN_DIGIT t[3][MAX_NN_DIGITS];
	NN_Assign(g(0), c, digits);
	NN_Assign(g(1), b, digits);
	i=1;
	while(!NN_Zero(g(i),digits)) {
		NN_Mod(g(iplus1), g(iminus1), digits, g(i), digits);
		i = iplus1;
	}
	NN_Assign(a , g(iminus1), digits);
}
/* Returns the significant length of a in bits.
	 Lengths: a[digits]. */
unsigned int NN_Bits (a, digits) NN_DIGIT *a;
unsigned int digits;
{
	if ((digits = NN_Digits (a, digits)) == 0)
		return (0);
	return ((digits - 1) * NN_DIGIT_BITS + NN_DigitBits (a[digits-1]));
}
#ifndef USEASM
/* Returns sign of a - b. */
int NN_Cmp (a, b, digits)
NN_DIGIT *a, *b;
unsigned int digits;
{
	if(digits) {
		do {
			digits--;
			if(*(a+digits) > *(b+digits))
				return(1);
			if(*(a+digits) < *(b+digits))
				return(-1);
		}while(digits);
	}
	return (0);
}
/* Returns nonzero iff a is zero. */
int NN_Zero (a, digits)
NN_DIGIT *a;
unsigned int digits;
{
	if(digits) {
		do {
			if(*a++)
				return(0);
		}while(--digits);
	}
	return (1);
}
/* Assigns a = b. */
void NN_Assign (a, b, digits)
NN_DIGIT *a, *b;
unsigned int digits;
{
	if(digits) {
		do {
			*a++ = *b++;
		}while(--digits);
	}
}
/* Returns the significant length of a in digits. */
unsigned int NN_Digits (a, digits) NN_DIGIT *a; unsigned int digits;
{
	if(digits) {
		digits--;
		do { if(*(a+digits)) break; }while(digits--);
		return(digits + 1);
	}
	return(digits);
}
/* Computes a = b + c. Returns carry.
	 Lengths: a[digits], b[digits], c[digits].
 */
NN_DIGIT NN_Add (a, b, c, digits) NN_DIGIT *a, *b, *c; unsigned int digits;
{
	NN_DIGIT temp, carry = 0;
	if(digits)
		do {
			if((temp = (*b++) + carry) < carry)
				temp = *c++;
            		else {      /* Patch to prevent bug for Sun CC */
               		 if((temp += *c) < *c) carry = 1; else carry = 0;
			 c++;
            		}
			*a++ = temp;
		}while(--digits);
	return (carry);
}
#endif
static NN_DIGIT subdigitmult(a, b, c, d, digits) NN_DIGIT *a, *b, c, *d; unsigned int digits;
{
	NN_DIGIT borrow, thigh, tlow;
	unsigned int i;
	borrow = 0;
	if(c != 0) {
		for(i = 0; i < digits; i++) {
			dmult(c, d[i], &thigh, &tlow);
			if((a[i] = b[i] - borrow) > (MAX_NN_DIGIT - borrow))
				borrow = 1;
			else
				borrow = 0;
			if((a[i] -= tlow) > (MAX_NN_DIGIT - tlow))
				borrow++;
			borrow += thigh;
		}
	}
	return (borrow);
}
/* Returns the significant length of a in bits, where a is a digit. */
static unsigned int NN_DigitBits (a) NN_DIGIT a;
{
	unsigned int i;
	for (i = 0; i < NN_DIGIT_BITS; i++, a >>= 1) if (a == 0) break;
	return (i);
}
/* Computes a * b, result stored in high and low. */
static void dmult( a, b, high, low) NN_DIGIT a, b; NN_DIGIT *high; NN_DIGIT *low;
{
	NN_HALF_DIGIT al, ah, bl, bh;
	NN_DIGIT m1, m2, m, ml, mh, carry = 0;
	al = (NN_HALF_DIGIT)LOW_HALF(a);
	ah = (NN_HALF_DIGIT)HIGH_HALF(a);
	bl = (NN_HALF_DIGIT)LOW_HALF(b);
	bh = (NN_HALF_DIGIT)HIGH_HALF(b);
	*low = (NN_DIGIT) al*bl;
	*high = (NN_DIGIT) ah*bh;
	m1 = (NN_DIGIT) al*bh;
	m2 = (NN_DIGIT) ah*bl;
	m = m1 + m2;
	if(m < m1)
        carry = 1L << (NN_DIGIT_BITS / 2);
	ml = (m & MAX_NN_HALF_DIGIT) << (NN_DIGIT_BITS / 2);
	mh = m >> (NN_DIGIT_BITS / 2);
	*low += ml;
	if(*low < ml) carry++;
	*high += carry + mh;
}
/*****************************************************************************
 *************************************************************** r_stdlib.c */
//BYTE *Copyright[] = { "Copyright (c) J.S.A.Kapp 94-96." };
#ifndef USE_ANSI
/* Secure memset routine */
#ifndef USEASM
void R_memset(output, value, len)
POINTER output;                 /* output block */
int value;                      /* value */
unsigned int len;               /* length of block */
{ if(len != 0) { do { *output++ = (unsigned char)value;	}while(--len != 0); }}
/* Secure memcpy routine */
void R_memcpy(output, input, len)
POINTER output;                 /* output block */
POINTER input;                  /* input block */
unsigned int len;               /* length of blocks */
{ if (len != 0) { do {	*output++ = *input++;	}while (--len != 0); } }
/* Secure memcmp routine */
int R_memcmp(Block1, Block2, len)
POINTER Block1;                 /* first block */
POINTER Block2;                 /* second block */
unsigned int len;               /* length of blocks */
{
	if(len != 0) {
		/* little trick in declaring vars */
		register const unsigned char *p1 = Block1, *p2 = Block2;
		do { if(*p1++ != *p2++)	return(*--p1 - *--p2);	}while(--len != 0);
	}
	return(0);
}
#endif /* USEASM */
#endif /* USE_ANSI */
/*****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************
 ********************************************************** finish includes */
static int rsapublicfunc PROTO_LIST((unsigned char *, unsigned int *, unsigned char *, unsigned int, R_RSA_PUBLIC_KEY *));
static int rsaprivatefunc PROTO_LIST((unsigned char *, unsigned int *, unsigned char *, unsigned int, R_RSA_PRIVATE_KEY *));

// RSA decryption, according to RSADSI's PKCS #1.
int RSAPublicDecrypt(output, outputLen, input, inputLen, publicKey)
unsigned char *output;          // output block 
unsigned int *outputLen;        // length of output block
unsigned char *input;           // input block
unsigned int inputLen;          // length of input block
R_RSA_PUBLIC_KEY *publicKey;    // RSA public key
{
	int status;
	unsigned char pkcsBlock[MAX_RSA_MODULUS_LEN];
	unsigned int i, modulusLen, pkcsBlockLen;
	// Generate publicKey->bits ourselves
#define pk(x) ((unsigned char)(publicKey->modulus[(x)]))
	status = 0;
	for ( i = 0; i < MAX_RSA_MODULUS_LEN; ++i ) {
		if      ( pk(i) >= 0x80 ) status = 8;
		else if ( pk(i) >= 0x40 ) status = 7;
		else if ( pk(i) >= 0x20 ) status = 6;
		else if ( pk(i) >= 0x10 ) status = 5;
		else if ( pk(i) >= 0x08 ) status = 4;
		else if ( pk(i) >= 0x04 ) status = 3;
		else if ( pk(i) >= 0x02 ) status = 2;
		else if ( pk(i) >= 0x01 ) status = 1;
		if ( status > 0 ) {
			status += ( 8 * ( MAX_RSA_MODULUS_LEN - i ) ) - 8;
			i = MAX_RSA_MODULUS_LEN;
		}
	}
	publicKey->bits = status;
	status = 0;
	modulusLen = (publicKey->bits + 7) / 8;
	if(inputLen > modulusLen)
		return(RE_LEN);
	status = rsapublicfunc(pkcsBlock, &pkcsBlockLen, input, inputLen, publicKey);
	if(status)
		return(status);
	if(pkcsBlockLen != modulusLen)
		return(RE_LEN);
	/* Require block type 1. */
	if((pkcsBlock[0] != 0) || (pkcsBlock[1] != 1))
	 return(RE_DATA);

	for(i = 2; i < modulusLen-1; i++)
		if(*(pkcsBlock+i) != 0xff)
			break;
	/* separator check */
	if(pkcsBlock[i++] != 0)
		return(RE_DATA);
	*outputLen = modulusLen - i;
	if(*outputLen + 11 > modulusLen)
		return(RE_DATA);
	R_memcpy((POINTER)output, (POINTER)&pkcsBlock[i], *outputLen);
	/* Clear sensitive information. */
	R_memset((POINTER)pkcsBlock, 0, sizeof(pkcsBlock));
	return(ID_OK);
}
// Raw RSA public-key operation. Output has same length as modulus.
//	 Requires input < modulus.
static int rsapublicfunc(output, outputLen, input, inputLen, publicKey)
unsigned char *output;          // output block
unsigned int *outputLen;        // length of output block
unsigned char *input;           // input block
unsigned int inputLen;          // length of input block
R_RSA_PUBLIC_KEY *publicKey;    // RSA public key
{
	NN_DIGIT c[MAX_NN_DIGITS], e[MAX_NN_DIGITS], m[MAX_NN_DIGITS],
		n[MAX_NN_DIGITS];
	unsigned int eDigits, nDigits;
		/* decode the required RSA function input data */
	NN_Decode(m, MAX_NN_DIGITS, input, inputLen);
	NN_Decode(n, MAX_NN_DIGITS, publicKey->modulus, MAX_RSA_MODULUS_LEN);
	NN_Decode(e, MAX_NN_DIGITS, publicKey->exponent, MAX_RSA_MODULUS_LEN);
	nDigits = NN_Digits(n, MAX_NN_DIGITS);
	eDigits = NN_Digits(e, MAX_NN_DIGITS);
	if(NN_Cmp(m, n, nDigits) >= 0)
		return(RE_DATA);
	*outputLen = (publicKey->bits + 7) / 8;
	/* Compute c = m^e mod n.  To perform actual RSA calc.*/
	NN_ModExp (c, m, e, eDigits, n, nDigits);
	/* encode output to standard form */
	NN_Encode (output, *outputLen, c, nDigits);
	/* Clear sensitive information. */
	R_memset((POINTER)c, 0, sizeof(c));
	R_memset((POINTER)m, 0, sizeof(m));
	return(ID_OK);
}
#endif
