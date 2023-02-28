/*
 * Produce output.
 */

#include <stdio.h>
#include "dm.h"

/*
 * DCOMMA must fit in a char and be distinguishable from
 * any valid digit (in the valid radix range of 2..35).
 */
#define DCOMMA        36
#define PR_CONTIN     '_'
#define PR_MALFORMED  '?'
#define SP            ' '
#define DEL           0x7F

typedef union {
	long long s;
	unsigned long long u;
} number;

static void printitem(struct format *f, number num);
static char * prchar(struct format *f, number num, int *widthp);
static char * prnum(struct format *f, number num, int *widthp);
static char * prcharnum(struct format *f, number num, int *widthp);
static void prspaces(int n);
extern int bigendian;
extern int color;

static char * color_ctl = "\e[33m";
static char * color_normal = "\e[m";

	static void
strcpy_color(char *buf, char *s)
{
	buf[0] = '\0';
	if (color)
		strcat(buf, color_ctl);
	strcat(buf, s);
	if (color)
		strcat(buf, color_normal);
}

/*
 * Print a string.
 */
	void
prstring(char *s)
{
	fputs(s, stdout);
}

/*
 * Pad with spaces on the left or right as required.
 */
	static void
prjust(struct format *f, char *s, int width) 
{
	if (f->flags & LEFTJUST) {
		prstring(s);
		prspaces(f->width - width);
	} else {
		prspaces(f->width - width);
		prstring(s);
	}
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
		int isize = (f->size > 0) ? f->size : 1;
		int spec_char = 0;
		if (len <= 0) {
			/* No more data in the buffer; just print spaces. */
			prspaces(f->width);
		} else {
			/* Extract the next number and print it. */
			num.u = 0;
			if (f->flags & UTF_8) {
				int usize = len;
				int uvalue = utf8_value(buf, &usize);
				if (uvalue == UTF_CONTIN)
					spec_char = PR_CONTIN;
				else if (uvalue == UTF_ERROR)
					spec_char = PR_MALFORMED;
				else
					num.u = uvalue;
			} else  {
				if ((f->flags & DM_BIG_ENDIAN) || (!(f->flags & DM_LITTLE_ENDIAN) && bigendian)) {
					for (i = 0;  i < isize;  i++)
						num.u = (256 * num.u) + buf[i];
				} else {
					for (i = isize-1;  i >= 0;  i--)
						num.u = (256 * num.u) + buf[i];
				}
			}
			if (spec_char) {
				char spec_str[] = { spec_char, '\0' };
				char buf[64];
				strcpy_color(buf, spec_str);
				prjust(f, buf, strlen(buf));
			} else {
				printitem(f, num);
			}
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
	if (f->radix == 1 || (f->flags & ASCHAR))
		s = prchar(f, num, &width);
	else
		s = prnum(f, num, &width);
	prjust(f, s, width);
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
		comma = 10000; /* more than the possible number of digits */
	do {
		digits[d++] = unum % f->radix;
		unum /= f->radix;
		if (--comma <= 0) {
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
	if (widthp != NULL)
		*widthp = s - buf;
	return (buf);
}

static char *aschar[] =
{
   "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
   "BS",  "HT",  "NL",  "VT",  "NP",  "CR",  "SO",  "SI",
   "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
   "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"
};

static char *cstyle[] =
{
   "\\0", NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,   
   "\\b", "\\t", "\\n", NULL,  "\\f", "\\r", NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  "\\e", NULL,  NULL,  NULL,  NULL
};

#define TABLESIZE(table) (sizeof(table)/sizeof(char *))


/*
 * Print a character as a number.
 */
	static char *
prcharnum(struct format *f, number num, int *widthp)
{
	struct format cformat = *f;
	cformat.size = 1;
	cformat.flags = ZEROPAD | (f->flags & (UPPERCASE));
	return (prnum(&cformat, num, widthp));
}

/*
 * Return the printable form of a character.
 */
	static char *
prchar(struct format *f, number num, int *widthp)
{
	int n = num.u;
	static char buf[64];
	char *s;

	/* if (f->flags & SIGNED) panic("prchar signed"); */

	int printable = (f->flags & UTF_8) ? utf8_is_printable(n) : (n >= 0x20 && n < 0x7f);
	if (printable) {
		int len;
		utf8_encode(n, buf, &len);
		buf[len] = '\0';
		*widthp = 1;
		return (buf);
	}

	if ((f->flags & ASCHAR) == 0) {
		/* Just print non-printables as ".". */
		s = ".";
	} else if ((f->flags & CSTYLE) &&
		n < TABLESIZE(cstyle) && cstyle[n] != NULL) {
		/* C-style escape sequences for certain non-printables. */
		s = cstyle[n];
	} else if ((f->flags & MNEMONIC) && n < TABLESIZE(aschar)) {
		/* Special mnemonic ASCII form for certain non-printables. */
		s =  aschar[n];
	} else if ((f->flags & MNEMONIC) && n == DEL) {
		/* Special mnemonic ASCII form; special case for DEL. */
		s = "DEL";
	} else {
		s = prcharnum(f, num, widthp);
	}
	*widthp = strlen(s);
	strcpy_color(buf, s);
	return buf;
}


/*
 * Print n spaces.
 */
	static void
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
	static unsigned long long
maxi(int size)
{
	switch (size)
	{
	case 8: return (0xffffffffffffffffLL);
	case 4: return (0xffffffff);
	case 2: return (0xffff);
	case 1: return (0xff);
	case -1: return (0x10ffff); // UTF-8
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
	if (radix == 1) /* Single character display */
		return (1);
	unsigned long long n = maxi(size);
	int ndig;
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
