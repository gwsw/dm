#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
	char radix;    /* Radix (base) of number representation
	                  Note: radix 1 means character printing */
	char size;     /* Size of numbers (1=byte, 2=word, 4=long) */
	char width;    /* Printable width of each output number */
	char zwidth;   /* Width to zero pad */
	char comma;    /* Spacing of commas within printed number */
	char col;      /* Column of this format; formats that are 
	                  directly under each other have the same column */
};

/* Flags */
#define	SIGNED		0x01	/* Interpret numbers as signed */
#define	LEFTJUST	0x02	/* Left justify in output */
#define	ZEROPAD		0x04	/* Pad with zeros */
#define	NOPRINT		0x08	/* Don't display at all */
#define	UPPERCASE	0x10	/* Uppercase hex letters */
#define	ASCHAR		0x20	/* Verbose ASCII */
#define	MNEMONIC	0x40	/* Mnemonic ASCII */
#define	CSTYLE		0x80	/* Alternate mnemonic ASCII */
#define	DOTCOMMA	0x100	/* Print dots (periods) instead of commas */
#define	BIGENDIAN	0x200	/* */
#define	UTF_8		0x400	/* UTF-8 */

void dumpfile(char *filename);
int getint(char **ss);
long getlong(char **ss);
int ndigits(int radix, int size);
void option(char *s);
int options(int argc, char *argv[]);
void panic(char *s);
void printbuf(struct format *f, u8 *buf, int size, int len);
void prspaces(int n);
void prstring(char *s);
unsigned long long maxi(int size);
void usage(char *s);
int defwidth(int radix, int size, int comma);
int is_wide_char(unsigned long ch);
int utf8_size(unsigned char ch);
