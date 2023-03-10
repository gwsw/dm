/*
 * dm - file dump utility
 *
 * dm, like od, dumps a file in various formats.
 * It has more options and more control of the output format than od.
 * The man page has details; here is a brief summary of the options:
 *  b  bytes
 *  w  words
 *  l  longwords
 *  x  hex
 *  o  octal
 *  d  decimal
 *  r# radix #
 *  X  uppercase
 *  c  ASCII
 *  C  detailed ASCII
 *  u  UTF-8
 *  U  detailed UTF-8 
 *  e  use C style escapes (with -C)
 *  m  use ASCII mnemonics (with -C)
 *  s  signed
 *  j  left justify
 *  z  zero pad
 *  p# set printing width to #
 *  ,# insert commas every # digits
 *  .# insert periods every # digits
 *  a  format applies to file addresses
 *  k  use color
 * Generally, each command line option sets up one display format.
 */

#include <stdio.h>
#include <stdlib.h>
#include "dm.h"

extern int nformat;
extern struct format format[];
extern struct format aformat;
extern int count;
extern int verbose;
extern off_t fileoffset;
extern int readoffset;
extern int bigendian;
extern int group_line;

	static int
is_bigendian(void)
{
	u32 one = 1;
	return (*(u8*)&one != 1);
}

	int
main(int argc, char *argv[])
{
	bigendian = is_bigendian();
	int arg = options(argc, argv);
	if (nformat == 0) {
		char *dm = getenv("DM");
		if (dm != NULL) {
			option(dm);
		} else {
			/* Use default format. */
			option("-xb");
			option("+c");
		}
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
	size_t last_len = 0;
	size_t rextra = 6;
	int didstar = 0;
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
		size_t len = 0;
		for (addr = 0;  addr < fileoffset;  addr += len) {
			len = (size_t) (fileoffset - addr);
			if (len > sizeof(buf))
				len = sizeof(buf);
			len = fread(buf, sizeof(char), len, f);
			if (len <= 0) {
				fprintf(stderr, "cannot read to %ld in %s\n",
					(long) fileoffset, filename);
				return;
			}
		}
	} else {
		/* Advance by seeking. */
		if (fseek(f, (long) fileoffset, 0)) {
			fprintf(stderr, "cannot seek to %ld in %s\n",
				(long) fileoffset, filename);
			return;
		}
		addr = fileoffset;
	}

	firstaddr = addr;
	size_t bufdata = 0;
	for (;; addr += count) {
		if (bufdata < count) {
			bufdata = 0;
		} else {
			memmove(buf, buf+count, bufdata-count);
			bufdata -= count;
		}
		ssize_t nread = fread(buf + bufdata, sizeof(char), count + rextra - bufdata, f);
		if (nread < 0) break;
		bufdata += nread;
		if (bufdata == 0) break;
		/* Fill the unused bytes with 0. */
		if (bufdata < count)
			memset(&buf[bufdata], 0, count-bufdata);
		/* line_len is amount to print on this line.
		 * Normally line_len==count unless there is not enough data in buf. */
		size_t line_len = bufdata;
		if (line_len > count) line_len = count;
		/* Duplicate of the previous line? */
		if (!verbose && addr != firstaddr && 
				line_len == last_len && eqbuf(buf, lastbuf, line_len)) {
			/* Just print an asterisk (unless we've already done so). */
			if (!didstar)
				prstring("*\n");
			didstar = 1;
			continue;
		}
		didstar = 0;
		last_len = line_len;
		/* Remember the current buffer. */
		memcpy(lastbuf, buf, line_len);

		/* Print the address, in the address format. */
		printbuf(&aformat, (u8*) &addr, sizeof(addr), sizeof(addr), sizeof(addr));

		/* Print the data, in all formats. */
		int fx;
		for (fx = 0;  fx < nformat;  fx++)
			printbuf(&format[fx], (u8*) buf, count, line_len, bufdata);
		if (group_line)
			prstring("\n");
	}
	/* Print the final address. */
	printbuf(&aformat, (u8*) &addr, sizeof(addr), sizeof(addr), sizeof(addr));
	prstring("\n");
	fclose(f);
}
