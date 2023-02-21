#include "dm.h"

struct wchar_range { unsigned long first, last; };
struct wchar_range_table { struct wchar_range *table; int count; };

int utf8_size(unsigned char ch)
{
	return 
		(ch & 0xE0) == 0xC0 ? 2 : 
		(ch & 0xF0) == 0xE0 ? 3 : 
		(ch & 0xF8) == 0xF0 ? 4 :
		(ch & 0xFC) == 0xF8 ? 5 : 
		(ch & 0xFE) == 0xFC ? 6 : 1;
}

static int is_in_table(unsigned long ch, struct wchar_range_table *table)
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

int is_wide_char(unsigned long ch)
{
	return is_in_table(ch, &wide_table);
}
