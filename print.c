/*
 * Produce output.
 */

#include <stdio.h>
#include "dm.h"

/*
 * DCOMMA must fit in a char and be distinguishable from
 * any valid digit (in the valid radix range of 2..35).
 */
#define	DCOMMA	(36)
#define PR_CONTIN  '_'

typedef union {
	long long s;
	unsigned long long u;
} number;

static void printitem(struct format *f, number num);
static char * prchar(struct format *f, number num, int *widthp);
static char * prnum(struct format *f, number num, int *widthp);
static void print_utf8(struct format *f, u8 *buf);
extern int bigendian;

/*
 * Print a string.
 */
	void
prstring(char *s)
{
	fputs(s, stdout);
}

/*
 * Print a buffer of data according to a given format.
 * size is the full size of the buffer.
 * len is the amount of data actually in the buffer.
 */
	void
printbuf(struct format *f, u8 *buf, int size, int len)
{
	number num;
	int i;

	if (f->flags & NOPRINT)
		/*
		 * This strange flag which says "don't print anything"
		 * is usually used only with the address format to 
		 * suppress addresses.
		 */
		return;

	while (size > 0) {
		char isize = f->size;
		if (len <= 0) {
			/* No more data in the buffer; just print spaces. */
			prspaces(f->width);
		} else if ((f->flags & UTF_8) && (buf[0] & 0x80)) {
			/* Extract next UTF-8 char and print it. */
			print_utf8(f, buf);
			isize = 1; /* just count 1 so we print the continuation bytes */
		} else {
			/* Extract the next number and print it. */
			num.u = 0;
			if (bigendian) { // FIXME (f->flags & BIGENDIAN)
				for (i = 0;  i < isize;  i++)
					num.u = (256 * num.u) + buf[i];
			} else {
				for (i = isize-1;  i >= 0;  i--)
					num.u = (256 * num.u) + buf[i];
			}
			printitem(f, num);
		}
		buf += isize;
		len -= isize;
		size -= isize;
		/*
		 * If there is another number after this one,
		 * print the "inter" string.
		 */
		if (size > 0)
			prstring(f->inter);
	}
	prstring(f->after);
}

/*
 * Print a single data item according to a given format.
 */
	static void
printitem(struct format *f, number num)
{
	char *s;
	int width;

	/*
	 * Get the printable form of the item.
	 */
	if (f->radix == 1 || (f->flags & (ASCHAR|UTF_8)))
		s = prchar(f, num, &width);
	else
		s = prnum(f, num, &width);

	/*
	 * Pad with spaces on the left or right as required.
	 */
	if (f->flags & LEFTJUST) {
		prstring(s);
		prspaces(f->width - width);
	} else {
		prspaces(f->width - width);
		prstring(s);
	}
}

	static void
print_utf8(struct format *f, u8 *buf)
{
	int pad = f->width - 1;
	if (!(f->flags & LEFTJUST))
		prspaces(pad);
	if ((buf[0] & 0xC0) == 0x80) { /* continuation byte */
		fputc(PR_CONTIN, stdout);
	} else { /* print full (multibyte) UTF-8 char */
		int isize =
			(buf[0] & 0xE0) == 0xC0 ? 2 : 
			(buf[0] & 0xF0) == 0xE0 ? 3 : 
			(buf[0] & 0xF8) == 0xF0 ? 4 : 1;
		int i;
		/* We can access all of isize, even if it is past buf len,
		 * because of rextra. */
		for (i = 0;  i < isize;  i++)
			fputc(buf[i], stdout);
	}
	if (f->flags & LEFTJUST)
		prspaces(pad);
}

/*
 * Return the printable form of a number.
 */
	static char *
prnum(struct format *f, number num, int *widthp)
{
	unsigned long unum;
	char *s;
	int d;
	int v;
	int width;
	int comma;
	int neg;
	char digits[70];
	static char buf[70];

	/*
	 * Get the raw unsigned number.
	 * We negate the number if it is already negative
	 * (and we are treating it as a signed number).
	 */
	if (!(f->flags & SIGNED)) {
		neg = 0;
		unum = num.u;
	} else if ((neg = (num.s < 0))) {
		unum = -(num.s);
	} else {
		unum = num.s;
	}

	/*
	 * Get the digits of the number, in the current radix.
	 * We continue until we run out of nonzero digits, and
	 * (if we are zero padding) we reach the maximum width.
	 * Always get at least one digit, even if the number is zero.
	 */
	if (f->flags & ZEROPAD)
		width = f->zwidth;
	else
		width = 0;

	d = 0;
	if ((comma = f->comma) == 0)
		comma = 10000;	/* more than the possible number of digits */
	do {
		digits[d++] = unum % f->radix;
		unum /= f->radix;
		if (--comma <= 0)
		{
			digits[d++] = DCOMMA;
			comma = f->comma;
		}
	} while (unum != 0 || d < width);  
	/* until (unum == 0 && d >= width) */

	if (digits[d-1] == DCOMMA)
		d--;

	/*
	 * Now start producing output.
	 * ("Output" just goes into the buffer for now.)
	 */
	s = buf;

	/* If signed, print the sign. */
	if (f->flags & SIGNED)
		*s++ = neg ? '-' : ' ';

	/* Print the digits of the number. */
	while (--d >= 0) {
		v = digits[d];
		if (v == DCOMMA)
			*s++ = (f->flags & DOTCOMMA) ? '.' : ',';
		else if (v <= 9)
			*s++ = v + '0';
		else if (f->flags & UPPERCASE)
			*s++ = v + 'A' - 10;
		else
			*s++ = v + 'a' - 10;
	}
	*s = '\0';
	*widthp = s - buf;
	return (buf);
}

static char *aschar[] =
{
	"NUL",	"SOH",	"STX",	"ETX",	"EOT",	"ENQ",	"ACK",	"BEL",
	"BS",	"HT",	"NL",	"VT",	"NP",	"CR",	"SO",	"SI",
	"DLE",	"DC1",	"DC2",	"DC3",	"DC4",	"NAK",	"SYN",	"ETB",
	"CAN",	"EM",	"SUB",	"ESC",	"FS",	"GS",	"RS",	"US"
};

static char *cstyle[] =
{
	"\\0",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	
	"\\b",	"\\t",	"\\n",	NULL,	"\\f",	"\\r",	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	"\\e",	NULL,	NULL,	NULL,	NULL
};

#define	TABLESIZE(table)	(sizeof(table)/sizeof(char *))

#define	SP	(' ')
#define	DEL	(0x7F)

/*
 * Return the printable form of a character.
 */
	static char *
prchar(struct format *f, number num, int *widthp)
{
	register int n;
	struct format cformat;
	static char buf[2];

	/* if (f->flags & SIGNED) panic("prchar signed"); */
	n = num.u & 0xFF;

	if (n >= SP && n < DEL) {
		/* Printable character. */
		buf[0] = n;
		buf[1] = '\0';
		*widthp = 1;
		return (buf);
	}

	if ((f->flags & ASCHAR) == 0) {
		/* Just print non-printables as ".". */
		buf[0] = '.';
		buf[1] = '\0';
		*widthp = 1;
		return (buf);
	}

	if ((f->flags & CSTYLE) &&
		n < TABLESIZE(cstyle) && cstyle[n] != NULL) {
		/* C-style escape sequences for certain non-printables. */
		*widthp = strlen(cstyle[n]);
		return (cstyle[n]);
	}

	if ((f->flags & MNEMONIC) && n < TABLESIZE(aschar)) {
		/* Special mnemonic ASCII form for certain non-printables. */
		*widthp = strlen(aschar[n]);
		return (aschar[n]);
	}

	if ((f->flags & MNEMONIC) && n == DEL) {
		/* Special mnemonic ASCII form; special case for DEL. */
		*widthp = 3;
		return ("DEL");
	}

	/* Print as a number. */
	cformat = *f;
	cformat.size = 1;
	cformat.flags = ZEROPAD | (f->flags & (UPPERCASE));
	return (prnum(&cformat, num, widthp));
}

/*
 * Print n spaces.
 */
	void
prspaces(int n)
{
	while (--n >= 0)
		prstring(" ");
}

/*
 * Return the default width (that is, the maximum required printing width)
 * for a given radix, size and comma spacing.
 */
	int
defwidth(int radix, int size, int comma)
{
	int width;
	width = ndigits(radix, size);
	if (comma)
		width += (width-1) / comma;
	return (width);
}

/*
 * Return the max (unsigned) value for a given size (byte, word, long).
 */
	unsigned long long
maxi(int size)
{
	switch (size)
	{
	case 8: return (0xffffffffffffffffLL);
	case 4: return (0xffffffff);
	case 2: return (0xffff);
	case 1: return (0xff);
	}
	panic("maxi");
	/*NOTREACHED*/
	return 0;
}

/*
 * Return the maximum number of digits required for a given radix and size.
 */
	int
ndigits(int radix, int size)
{
	unsigned long long n;
	int ndig;

	if (radix == 1)
		/* Simple ASCII character display */
		return (1);

	n = maxi(size);
	for (ndig = 0;  n != 0;  ndig++, n /= radix)
		continue;
	return (ndig);
}

	void
panic(char *s)
{
	fprintf(stderr, "*** dm program error: %s ***\n", s);
	exit(2);
}
