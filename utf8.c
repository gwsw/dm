#include "dm.h"

struct wchar_range { unsigned long first, last; };
struct wchar_range_table { struct wchar_range *table; int count; };

	int
utf8_size(u8 ch)
{
	if ((ch & 0x80) == 0)
		return 1;
	return 
		(ch & 0xE0) == 0xC0 ? 2 : 
		(ch & 0xF0) == 0xE0 ? 3 : 
		(ch & 0xF8) == 0xF0 ? 4 :
		//(ch & 0xFC) == 0xF8 ? 5 : 
		//(ch & 0xFE) == 0xFC ? 6 : 
		UTF_ERROR;
}

	int
utf8_value(u8 *buf, int *plen)
{
	if (utf8_is_contin(*buf))
		return UTF_CONTIN;
	int usize = utf8_size(*buf);
	if (usize < 0 || usize > *plen)
		return UTF_ERROR;
	*plen = usize;
	int uvalue = 0;
	if (usize == 1) {
		uvalue = *buf;
	} else {
		int umask = (1 << (7-usize)) - 1;
		int first = 1;
		while (usize-- > 0) {
			if (!first && !utf8_is_contin(*buf))
				return UTF_ERROR;
			uvalue = (uvalue << 6) | (*buf++ & umask);
			umask = 0x3F;
			first = 0;
		}
	}
	return uvalue;
}

	void
utf8_encode(int value, u8 *buf, int *plen)
{
	u8* obuf = buf;
	if (value < 0x80) {
		*buf++ = value;
	} else if (value < 0x800) {
		*buf++ = 0xC0 | (value >> 6);
		*buf++ = 0x80 | (value & 0x3F);
	} else if (value < 0x10000) {
		*buf++ = 0xE0 | (value >> 12);
		*buf++ = 0x80 | ((value >> 6) & 0x3F);
		*buf++ = 0x80 | (value & 0x3F);
	} else if (value < 0x110000) {
		*buf++ = 0xF0 | (value >> 18);
		*buf++ = 0x80 | ((value >> 12) & 0x3F);
		*buf++ = 0x80 | ((value >> 6) & 0x3F);
		*buf++ = 0x80 | (value & 0x3F);
	} else {
	}
	if (plen != NULL) *plen = buf - obuf;
}

	int
utf8_is_contin(u8 ch)
{
	return (ch & 0xC0) == 0x80;
}

	static int
is_in_table(unsigned long ch, struct wchar_range_table *table)
{
	int hi;
	int lo;

	/* Binary search in the table. */
	if (table->table == NULL || table->count == 0 || ch < table->table[0].first)
		return 0;
	lo = 0;
	hi = table->count - 1;
	while (lo <= hi)
	{
		int mid = (lo + hi) / 2;
		if (ch > table->table[mid].last)
			lo = mid + 1;
		else if (ch < table->table[mid].first)
			hi = mid - 1;
		else
			return 1;
	}
	return 0;
}


#define DECLARE_RANGE_TABLE_START(name) \
	static struct wchar_range name##_array[] = {
#define DECLARE_RANGE_TABLE_END(name) \
	}; struct wchar_range_table name##_table = { name##_array, sizeof(name##_array)/sizeof(*name##_array) };

DECLARE_RANGE_TABLE_START(wide)
#include "wide.uni"
DECLARE_RANGE_TABLE_END(wide)

	int
utf8_is_wide(unsigned long ch)
{
	return is_in_table(ch, &wide_table);
}

	int
utf8_is_printable(unsigned long ch)
{
return ch >= 0x20;
	return ch >= ' ' && ch < 0x7f;
}
