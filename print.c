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
static int print_utf8_char(struct format *f, u8 *buf);
static int print_utf8_num(struct format *f, u8 *buf);
static void prspaces(int n);
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
	int skip = 0;

	if (f->flags & NOPRINT)
		/*
		 * This strange flag which says "don't print anything"
		 * is usually used only with the address format to 
		 * suppress addresses.
		 */
		return;

	while (size > 0) {
		char isize = (f->size > 0) ? f->size : 1;
		if (len <= 0) {
			/* No more data in the buffer; just print spaces. */
			prspaces(f->width);
		} else if (skip > 0) {
			char contin[] = { PR_CONTIN, '\0' };
			prjust(f, contin, 1);
			isize = 0;
			--skip;
		} else if (f->flags & UTF_8) {
			/* Extract next UTF-8 char and print it. */
			if (f->radix == 1) {
				print_utf8_char(f, buf);
				isize = 1; /* just count 1 so we print the continuation bytes */
			} else {
				isize = print_utf8_num(f, buf);
			}
			skip = isize - 1;
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
	prjust(f, s, width);
}

/*
 * Print multibyte UTF-8 char.
 */
	static int
print_utf8_char(struct format *f, u8 *buf)
{
	u8 obytes[8];
	int b = 0;
	char *out = (char *) obytes;
	unsigned long uvalue = 0;
	int usize = 1;
	int width = 1;
	if (utf8_is_contin(buf[0])) {
		obytes[b++] = uvalue = PR_CONTIN;
	} else { /* print full (multibyte) UTF-8 char */
		usize = sizeof(buf);
		uvalue = utf8_value(buf, &usize);
		if (uvalue < 0) {
			obytes[b++] = PR_MALFORMED;
		} else if (uvalue < SP) {
			number num;
			num.u = uvalue;
			out = prcharnum(f, num, NULL);
			width = strlen(out);
		} else {
			memcpy(obytes, buf, usize);
			b = usize;
			width = is_wide_char(uvalue) ? 2 : 1;
		}
	}
	obytes[b] = '\0';
	prjust(f, out, width);
	return usize;
}

/*
 * Print multibyte UTF-8 char.
 */
	static int
print_utf8_num(struct format *f, u8 *buf)
{
	char out[64];
	int usize;
	if (utf8_is_contin(buf[0])) {
		out[0] = PR_CONTIN;
		out[1] = '\0';
		usize = 1;
	} else {
		number num;
		usize = sizeof(buf);
		int uvalue = utf8_value(buf, &usize);
		if (uvalue < 0) {
			usize = 1;
			num.u = buf[0];
			snprintf(out, sizeof(out), "<%s>?", prnum(f, num, NULL));
		} else {
			num.u = uvalue;
			snprintf(out, sizeof(out), "%s", prnum(f, num, NULL));
		}
	}
	prjust(f, out, strlen(out));
	return usize;
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
	struct format cformat;

	cformat = *f;
	cformat.size = 1;
	cformat.radix = 16;
	cformat.flags = ZEROPAD | (f->flags & (UPPERCASE));
	return (prnum(&cformat, num, widthp));
}

/*
 * Return the printable form of a character.
 */
	static char *
prchar(struct format *f, number num, int *widthp)
{
	int n;
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

	return prcharnum(f, num, widthp);
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
	unsigned long long n;
	int ndig;

	if (radix == 1)
		/* Single character display */
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
