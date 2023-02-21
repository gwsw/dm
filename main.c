/*
 *	dm - file dump utility
 *
 * dm, like od, dumps a file in various formats.
 * It has more options and more control of the output format than od.
 * The man page has details; here is a brief summary of the options:
 *	b  bytes
 *	w  words
 *	l  longwords
 *	x  hex
 *	o  octal
 *	d  decimal
 *	r# radix #
 *	u  uppercase
 *	U  UTF-8
 *	X  same as xu
 *	c  characters
 *	C  verbose characters
 *	e  use C style escapes (with -C)
 *	m  use ASCII mnemonics (with -C)
 *	s  signed
 *	j  left justify
 *	z  zero pad
 *	p# set printing width to #
 *	,# insert commas every # digits
 *	.# insert periods every # digits
 *	a  format applies to file addresses
 * Generally, each command line option sets up one display format.
 */

#include <stdio.h>
#include "dm.h"

extern int nformat;
extern struct format format[];
extern struct format aformat;
extern int count;
extern int verbose;
extern off_t fileoffset;
extern int readoffset;
extern int bigendian;

static int is_bigendian()
{
	u32 one = 1;
	return (*(u8*)&one != 1);
}

int main(argc, argv)
	int argc;
	char *argv[];
{
	int arg;

	bigendian = is_bigendian();
	arg = options(argc, argv);
	if (nformat == 0) {
		/* Use default format. */
		option("-xb");
		option("+c");
	}
	if (arg == 0)
		/* Standard input */
		dumpfile("-");
	else for (arg = argc - arg;  arg < argc;  arg++)
		dumpfile(argv[arg]);

	exit(0);
}

/*
 * Dump an entire file.
 */
	void
dumpfile(char *filename)
{
	FILE *f;
	size_t len;
	size_t last_len;
	int didstar;
	off_t addr;
	off_t firstaddr;
	char buf[MAXLINESIZE];
	char lastbuf[MAXLINESIZE];

	if (strcmp(filename, "-") == 0) {
		/* Standard input */
		f = stdin;
		filename = "standard input";
	} else if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "cannot open <%s>\n", filename);
		return;
	}

	/*
	 * Advance to the proper file offset.
	 * We do this one of two ways:
	 *  by reading the file until we reach the desired offset,
	 *  or by seeking directly to the desired offset.
	 */
	if (fileoffset == 0) {
		/* No need to advance. */
		addr = 0;
	} else if (readoffset) {
		/* Advance by reading. */
		for (addr = 0;  addr < fileoffset;  addr += len)
		{
			len = (size_t) (fileoffset - addr);
			if (len > sizeof(buf))
				len = sizeof(buf);
			len = fread(buf, sizeof(char), len, f);
			if (len <= 0)
			{
				fprintf(stderr, "cannot read to %ld in %s\n",
					(long) fileoffset, filename);
				return;
			}
		}
	} else {
		/* Advance by seeking. */
		if (fseek(f, (long) fileoffset, 0))
		{
			fprintf(stderr, "cannot seek to %ld in %s\n",
				(long) fileoffset, filename);
			return;
		}
		addr = fileoffset;
	}

	firstaddr = addr;
	for ( ;  (len = fread(buf, sizeof(char), count, f)) > 0;  addr += len) {
		size_t i;
		/* Fill the unused bytes with 0. */
		for (i = len;  i < count;  i++)
			buf[i] = 0;

		/* Duplicate of the previous line? */
		if (!verbose && addr != firstaddr && 
				len == last_len && eqbuf(buf, lastbuf, len)) {
			/* Just print an asterisk (unless we've already done so). */
			if (!didstar)
				prstring("*\n");
			didstar = 1;
			continue;
		}
		didstar = 0;
		last_len = len;
		/* Remember the current buffer. */
		for (i = 0;  i < len;  i++)
			lastbuf[i] = buf[i];

		/* Print the address, in the address format. */
		printbuf(&aformat, (u8*) &addr, sizeof(addr), sizeof(addr));

		/* Print the data, in all formats. */
		for (i = 0;  i < nformat;  i++)
			printbuf(&format[i], buf, count, len);
	}
	/* Print the final address. */
	printbuf(&aformat, (u8*) &addr, sizeof(addr), sizeof(addr));
	prstring("\n");
	fclose(f);
}
