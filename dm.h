#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define version "1.3"

/*
 * Integral types for byte (8 bit), word (16 bit) and long (32 bit).
 * Both signed and unsigned types are specified here.
 */
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

#define UTF_ERROR   -1
#define UTF_CONTIN  -2

/*
 * Max number of formats.
 */
#define NFORMAT		 16

/*
 * Max bytes per input line.
 */
#define	MAXLINESIZE	 128

#ifndef NULL
#define	NULL		0
#endif

/*
 * eqbuf(buf1,buf2,len) is used for comparing two memory buffers.
 * Only comparison for equality is needed
 * (that is, less-than / greater-than is not needed).
 */
#define	eqbuf(b1,b2,len)	(memcmp((b1),(b2),(len)) == 0)

struct format
{
	char *after;   /* String to print after all the numbers in a line */
	char *inter;   /* String to print between numbers in a line */
	short flags;   /* Flags: see below */
	int radix;     /* Radix (base) of number representation
	                  Note: radix 1 means character printing */
	int size;      /* Size of numbers (1=byte, 2=word, 4=long) */
	int width;     /* Printable width of each output number */
	int zwidth;    /* Width to zero pad */
	int comma;     /* Spacing of commas within printed number */
	int col;       /* Column of this format; formats that are 
	                  directly under each other have the same column */
};

/* Flags */
#define SIGNED           (1<< 0)  /* Interpret numbers as signed */
#define LEFTJUST         (1<< 1)  /* Left justify in output */
#define ZEROPAD          (1<< 2)  /* Pad with zeros */
#define NOPRINT          (1<< 3)  /* Don't display at all */
#define UPPERCASE        (1<< 4)  /* Uppercase hex letters */
#define ASCHAR           (1<< 5)  /* Verbose ASCII */
#define MNEMONIC         (1<< 6)  /* Mnemonic ASCII */
#define CSTYLE           (1<< 7)  /* Alternate mnemonic ASCII */
#define DOTCOMMA         (1<< 8)  /* Print dots (periods) instead of commas */
#define DM_BIG_ENDIAN    (1<< 9)  /* Big-endian */
#define DM_LITTLE_ENDIAN (1<< 10) /* Little-endian */
#define UTF_8            (1<< 11) /* UTF-8 chars */
#define DM_CODEPT        (1<< 12) /* UTF-8 codepoints */

void dumpfile(char *filename);
int ndigits(int radix, int size);
void option(char *s);
int options(int argc, char *argv[]);
void panic(char *s);
void printbuf(struct format *f, u8 *buf, ssize_t size, ssize_t len, ssize_t rlen);
void prstring(char *s);
void usage(char *s);
int defwidth(int radix, int size, int comma);
int utf8_size(u8 ch);
int utf8_is_contin(u8 ch);
int utf8_value(u8 *buf, int *plen);
int utf8_is_wide(unsigned long ch);
int utf8_is_printable(unsigned long ch);
void utf8_encode(int value, u8 *buf, int *plen);
