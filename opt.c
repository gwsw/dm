/*
 * Process command line options.
 */

#include <stdio.h>
#include "dm.h"

char *version = "1.2";
struct format format[NFORMAT];	/* All data formats */
struct format aformat;		/* Address format */
int nformat = 0;		/* Number of formats in format[] */
int count = 16;			/* Count of bytes per line */
int verbose = 0;		/* Show all data */
long fileoffset = 0L;		/* Starting offset in the input file */
int readoffset = 0;		/* Read rather than seek to fileoffset */
int bigendian = 0;

/*
 * The "default" format.
 * Default format is used for any unspecified attributes of a format.
 * Various parts of the default format may be changed by a -- option.
 */
static struct format def = {
	NULL, NULL,
	0,		/* Default flags = (none) */
	16,		/* Default radix = hex */
	1,		/* Default size = byte */
	0,		/* width */
	0,		/* zwidth */
	0,		/* comma */
	0		/* col */
};

static char addrtab[64];

/* usage error messages */
char DUP_SIZE[] =	"more than one SIZE option in a format";
char DUP_RADIX[] = 	"more than one RADIX option in a format";

static void adjcol();
static void setaddrtab();
static void fixaformat();

/*
 * Parse command line options.
 */
	int
options(int argc, char *argv[])
{
	char *s;

	while (--argc > 0)
	{
		s = *++argv;
		if (*s != '-' && *s != '+')
			break;
		/*
		 * Special case: if first option starts with +,
		 * pretend there is an empty option before it.
		 * This will set up the default format first, then
		 * append the format specified by the + option.
		 */
		if (*s == '+' && nformat == 0)
			option("-");
		option(s);
	}

	fixaformat();
	setaddrtab();
	adjcol();

	return (argc);
}

/*
 * Initialize the address format.
 * It may already be partially initialized by a -a option.
 */
	void
fixaformat()
{
	aformat.size = sizeof(off_t);
	aformat.after = ": ";
	aformat.inter = "";

	if (aformat.radix == 0)
		aformat.radix = def.radix;
	if (aformat.comma == 0)
		aformat.comma = def.comma;

	/*
	 * Keep only the flags which are meaningful for addresses.
	 */
	aformat.flags |= def.flags;
	aformat.flags &= NOPRINT|LEFTJUST|ZEROPAD|UPPERCASE|DOTCOMMA;

	/*
	 * Set up the width and zwidth.
	 */
	aformat.zwidth = defwidth(aformat.radix, aformat.size, aformat.comma);
	if (aformat.width == 0)
		aformat.width = aformat.zwidth;
	if (aformat.width > 10) aformat.width = 10; // FIXME??
	if (aformat.zwidth > aformat.width)
		aformat.zwidth = aformat.width;
}

/*
 * Parse a single command line option.
 * A single option generally sets up a single data format.
 * Exceptions are some options like -n which don't apply to data formats;
 * -a which sets up the address format;
 * and -- which sets up defaults for all subsequent formats.
 */
	void
option(char *s)
{
	struct format *f;
	int size;
	int radix;
	int flags;
	int addr;
	int width;
	int zwidth;
	int comma;
	char optchar;
	char *after;
	char *inter;

	optchar = *s++;
	if (*s == '-')
	{
		/*
		 * Option starts with double "-".
		 */
		optchar = '=';
		s++;
	}
	flags = 0;
	size = 0;
	radix = 0;
	width = 0;
	comma = 0;
	after = NULL;
	inter = NULL;
	addr = 0;

	while (*s != '\0')  switch (*s++)
	{
	case 'a':	/* Applies to address, not data */
		addr = 1;
		break;
	case 'b':	/* 8 bit size */
		if (size)
			usage(DUP_SIZE);
		size = 1;
		break;
	case 'c':	/* Character (ASCII) */
		if (size)
			usage(DUP_SIZE);
		if (radix)
			usage(DUP_RADIX);
		radix = 1;
		size = 1;
		inter = "";
		break;
	case 'C':	/* Character (expanded ASCII) */
		if (size)
			usage(DUP_SIZE);
		size = 1;
		flags |= ASCHAR;
		break;
	case 'd':	/* Radix 10 (decimal) */
		if (radix)
			usage(DUP_RADIX);
		radix = 10;
		break;
	case 'e':	/* Print C style escape sequences for characters */
		flags |= CSTYLE;
		break;
	case 'F':	/* Set initial file offset */
		readoffset = 1;
		/* FALLTHRU */
	case 'f':	/* Set initial file offset */
		fileoffset = getlong(&s);
		if (*s != '\0')
			usage("extra characters in -f option");
		return;
	case 'j':	/* Left justify */
		flags |= LEFTJUST;
		break;
	case 'l':	/* 32 bit size */
		if (size)
			usage(DUP_SIZE);
		size = 4;
		break;
	case 'L':	/* 64 bit size */
		if (size)
			usage(DUP_SIZE);
		size = 8;
		break;
	case 'm':	/* Mnemonic ASCII */
		flags |= MNEMONIC;
		break;
	case 'n':	/* Set count (bytes per line) */
		count = getint(&s);
		if (*s != '\0')
			usage("extra characters in -n option");
		if (count < 1 || count > MAXLINESIZE)
			usage("illegal value for -n option");
		return;
	case 'N':	/* Don't print; useful with -a */
		flags |= NOPRINT;
		break;
	case 'o':	/* Radix 8 (octal) */
		if (radix)
			usage(DUP_RADIX);
		radix = 8;
		break;
	case 'p':	/* Set printing width */
		width = getint(&s);
		break;
	case 'q':
		bigendian = 0;
		break;
	case 'Q':
		bigendian = 1;
		break;
	case 'r':	/* Set arbitrary radix */
		if (radix)
			usage(DUP_RADIX);
		radix = getint(&s);
		if (radix < 2 || radix > 36)
			usage("invalid radix");
		break;
	case 's':	/* Signed numbers */
		flags |= SIGNED;
		break;
	case 'u':	/* Use uppercase for alphabetic digits */
		flags |= UPPERCASE;
		break;
	case 'U':	/* UTF-8 */
		flags |= UTF_8;
		break;
	case 'v':
		verbose = 1;
		return;
	case 'V':
		printf("dm version %s\n", version);
		exit(0);
	case 'w':	/* 16 bit size */
		if (size)
			usage(DUP_SIZE);
		size = 2;
		break;
	case 'X':	/* Radix 16 (hex) with uppercase */
		flags |= UPPERCASE;
		/* FALLTHRU */
	case 'x':	/* Radix 16 (hex) */
		if (radix)
			usage(DUP_RADIX);
		radix = 16;
		break;
	case 'z':	/* Zero pad */
		flags |= ZEROPAD;
		break;
	case '.':
		flags |= DOTCOMMA;
		/* fall thru */
	case ',':
		comma = getint(&s);
		break;
	case '?':
		usage(NULL);
	default:
		usage("illegal option letter");
	}

	if (optchar == '=')
	{
		/*
		 * The option started with "--".
		 * Just change some defaults; don't set up a format.
		 */
		if (addr)
			usage("cannot use -a in a default (--) option");
		if (width != 0)
			usage("cannot set default for -p");
		if (size != 0)
			def.size = size;
		if (radix != 0)
			def.radix = radix;
		if (flags != 0)
			def.flags = flags;
		if (comma != 0)
			def.comma = comma;
		return;
	}

	/*
	 * Set up the format structure.
	 */
	if (addr)
	{
		/*
		 * Don't fill in any defaults for the address format.
		 * We take care of that later, in fixaformat().
		 */
		f = &aformat;
		if (radix == 1 || (flags & (ASCHAR|MNEMONIC|CSTYLE)))
			usage("invalid option used with -a");
		zwidth = 0;
	} else
	{
		/*
		 * Fill in defaults for anything not specified.
		 */
		if (size == 0)
			size = def.size;
		if (radix == 0)
			radix = def.radix;
		if (comma == 0)
			comma = def.comma;
		flags |= def.flags;
		if (radix == 1 || (flags & ASCHAR))
			flags &= ~SIGNED;
		if (after == NULL)
			after = "\n";
		if (inter == NULL)
			inter = " ";
		zwidth = defwidth(radix, size, comma);
		if (width == 0)
		{
			/*
			 * Set up printing width to be just big enough to
			 * hold the widest string we'll ever need to print.
			 */
			width = defwidth(radix, size, comma);
			if (flags & SIGNED)
				/* Add one for a possible minus sign. */
				width++;
			if ((flags & (ASCHAR|MNEMONIC)) == (ASCHAR|MNEMONIC) &&
				width < 3)
				/*
				 * Need at least 3 printing positions to
				 * display ASCII mnemonics.
				 */
				zwidth = width = 3;
			if ((flags & (ASCHAR|CSTYLE)) == (ASCHAR|CSTYLE) &&
				width < 2)
				/*
				 * Need at least 2 printing positions to
				 * display C style mnemonics.
				 */
				zwidth = width = 2;
		}

		/*
		 * Figure out the column for this format.
		 *
		 * This is a "logical column" assigned as follows:
		 * The first format on each line is column 0.
		 * Any format immediately to the right of a column 0
		 * format is column 1, and so on.
		 */
		if (nformat == 0)
		{
			/*
			 * The column of the first format must be 0.
			 */
			format[nformat].col = 0;
		} else if (optchar == '+')
		{
			/*
			 * Display NEXT TO the previous format.
			 * Set the previous format's "after" string
			 * to spaces, and set our column to one more 
			 * than the previous format's column.
			 */
			format[nformat-1].after = "   ";
			format[nformat].col = format[nformat-1].col + 1;
		} else
		{
			/*
			 * Display UNDER the previous format
			 * (that is, at the start of the next line).
			 * Set the previous format's "after" string
			 * to a newline and some spaces, and set
			 * our column to 0.
			 */
			format[nformat-1].after = addrtab;
			format[nformat].col = 0;
		}
		f = &format[nformat++];
		if (nformat > NFORMAT)
			usage("too many formats");
	}

	/*
	 * Set up the new format structure.
	 */
	f->radix = radix;
	f->size = size;
	f->width = width;
	f->zwidth = zwidth;
	f->flags = flags;
	f->comma = comma;
	f->after = after;
	f->inter = inter;
}

/*
 * Set up the addrtab string.
 * addrtab is used as the "after" string of the last format 
 * in a printable line, to print a newline followed by enough
 * spaces to tab past the address displayed on the first line.
 */
	static void
setaddrtab()
{
	int width;
	int i;

	addrtab[0] = '\n';
	/*
	 * Append enough spaces to equal the width of the address.
	 */
	width = aformat.width + strlen(aformat.after);
	for (i = 0;  i < width;  i++)
		addrtab[i+1] = ' ';
	addrtab[i+1] = '\0';
}


/*
 * Adjust the width (printable size) of each format
 * to make the columns line up nicely.
 */
	static void
adjcol()
{
	int col;
	struct format *f;
	int found;
	int minsize;
	int maxwidth8;
	int width8;

	for (col = 0; ; col++)
	{
		/*
		 * Find the smallest size and the largest width in this column.
		 * Actually, we don't look at the width, but the width8:
		 * the printable width of 8 bytes of data (whereas
		 * width is the printable size of "size" bytes of data).
		 * This lets us compare formats which have different sizes.
		 * We also count any trailing space (f->inter) in the width8.
		 */
		found = 0;
		minsize = 8;
		maxwidth8 = 0;

		for (f = format;  f < &format[nformat];  f++)
			if (f->col == col)
			{
				found++;
				if (f->size < minsize)
					minsize = f->size;
				width8 = (8 / f->size) * 
						(f->width + strlen(f->inter));
				if (width8 > maxwidth8)
					maxwidth8 = width8;
			}

		if (!found)
			/*
			 * Nothing in this column; we're done.
			 */
			return;

		/*
		 * Now round up the max width8 to be divisible into
		 * pieces as required by the min size.
		 */
		minsize = 8 / minsize;
		maxwidth8 = (maxwidth8 + minsize - 1) / minsize;
		maxwidth8 *= minsize;

		/*
		 * Run thru again, adjusting (rounding up) the width
		 * for each format in this column.
		 */
		for (f = format;  f < &format[nformat];  f++)
			if (f->col == col)
			{
				width8 = 8 / f->size;
				f->width = (maxwidth8 / width8) - 
						strlen(f->inter);
				if (strlen(f->inter) == 0 && f->width > 1)
				{
					/*
					 * Ugly kludge to handle characters:
					 * Normally, -c format has the inter
					 * string empty and width == 1.
					 * If we have adjusted the width to
					 * be > 1, then use the inter string
					 * for one of the spaces.
					 * This makes the chars line up
					 * better with other formats.
					 */
					f->inter = " ";
					f->width--;
				}
			}
	}
}

/*
 * Get the value of a single digit of an integer.
 */
	static int
gdigit(int ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A' + 10);
	return (-1);
}

/*
 * Parse an integer.
 */
	long
getlong(char **ss)
{
	char *s = *ss;
	long n;
	int radix;
	int v;

	/*
	 * Default radix is decimal.
	 */
	radix = 10;

	if (*s == '0' && s[1] != '\0')
	{
		/*
		 * If it starts with a 0, we use an alternate radix.
		 * Plain zero means octal.  0x means hex.
		 */
		s++;
		radix = 8;
		if (*s == 'x' || *s == 'X')
		{
			s++;
			radix = 16;
		}
	}

	/*
	 * Parse the digits of the number.
	 */
	n = 0;
	while ((v = gdigit(*s)) >= 0 && v < radix)
	{
		n = (radix * n) + v;
		s++;
	}
	if (s == *ss)
		usage("missing number");

	/*
	 * Followed by "k" means multiply by 1024.
	 */
	if (*s == 'k' || *s == 'K')
	{
		s++;
		n *= 1024;
	}
	*ss = s;
	return (n);
}

	int
getint(char **ss)
{
	return ((int) getlong(ss));
}

	void
usage(char *s)
{
	if (s != NULL)
		fprintf(stderr, "dm: %s\n", s);

	fprintf(stderr, "usage: dm [-n#][-v][-f#][-F#] [--<fmt>] [-aN<fmt>] [[-+]<fmt>]... [file]...\n");
	fprintf(stderr, "           -n#      bytes per line\n");
	fprintf(stderr, "           -v       don't skip repeated lines\n");
	fprintf(stderr, "           -f#      skip to offset #\n");
	fprintf(stderr, "           -F#      seek to offset #\n");
	fprintf(stderr, "           --<fmt>  set default format\n");
	fprintf(stderr, "           -a<fmt>  format file addresses\n");
	fprintf(stderr, "           -aN      suppress file addresses\n");
	fprintf(stderr, "           +<fmt>   format on same line\n");
	fprintf(stderr, "           -<fmt>   format on new line\n");
	fprintf(stderr, "<fmt> is:\n");
	fprintf(stderr, "      -b bytes   -c char,dot   -x  hex        -j  left justify\n");
	fprintf(stderr, "      -w words   -C char,num   -d  decimal    -z  zero pad\n");
	fprintf(stderr, "      -l longs   -m mnemonic   -o  octal      -p# printing width #\n");
	fprintf(stderr, "      -s signed  -e C-escape   -r# radix #    -,# comma every # digits\n");
	fprintf(stderr, "                 -U UTF-8      -u  uppercase  -.# dot every # digits\n");
	exit(1);
}
