#include "chardet.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <iconv.h>

#define MAX_BUFSIZE		512

typedef unsigned char guchar;
typedef unsigned gunichar;
#define G_UNLIKELY(expr) (expr)

#define CONTINUATION_CHAR \
	do { \
		if ((*(guchar *)p & 0xc0) != 0x80) /* 10xxxxxx */ \
			goto error; \
		val <<= 6; \
		val |= (*(guchar *)p) & 0x3f; \
	} while(0)

#define UNICODE_VALID(Char)                  \
	((Char) < 0x110000 &&                    \
	(((Char) & 0xFFFFF800) != 0xD800) &&     \
	((Char) < 0xFDD0 || (Char) > 0xFDEF) &&  \
	((Char) & 0xFFFE) != 0xFFFE)


static int
utf8_validate (const char *str)
{
	gunichar val = 0;
	gunichar min = 0;
	const char *p;

	for (p = str; *p; p++) {
		if (*(guchar *)p < 128)
			/* done */;
		else {
			const char *last;

			last = p;
			if ((*(guchar *)p & 0xe0) == 0xc0) { /* 110xxxxx */
				if (G_UNLIKELY ((*(guchar *)p & 0x1e) == 0))
					goto error;
				p++;
				if (G_UNLIKELY ((*(guchar *)p & 0xc0) != 0x80)) /* 10xxxxxx */
					goto error;
			} else {
				if ((*(guchar *)p & 0xf0) == 0xe0) { /* 1110xxxx */
					min = (1 << 11);
					val = *(guchar *)p & 0x0f;
					goto TWO_REMAINING;
				} else if ((*(guchar *)p & 0xf8) == 0xf0) { /* 11110xxx */
					min = (1 << 16);
					val = *(guchar *)p & 0x07;
				} else
					goto error;

				p++;
				CONTINUATION_CHAR;
TWO_REMAINING:
				p++;
				CONTINUATION_CHAR;
				p++;
				CONTINUATION_CHAR;

				if (G_UNLIKELY (val < min))
					goto error;

				if (G_UNLIKELY (!UNICODE_VALID(val)))
					goto error;
			}

			continue;

error:
			return 0;
		}
	}

	return *p == 0;
}

static char *
utf8_find_next_char (const char *p, const char *end)
{
	if (*p) {
		if (end)
			for (++p; p < end && (*p & 0xc0) == 0x80; ++p)
				;
		else
			for (++p; (*p & 0xc0) == 0x80; ++p)
				;
	}
	return (p == end) ? NULL : (char *)p;
}


static char *
simple_recode(const char *str, const char *from, const char *to, char *buf)
{
	char *in = (char*)str, *out = buf;
	size_t inb, outb;
	iconv_t h = iconv_open(to, from);
	inb = strlen(str);
	outb = MAX_BUFSIZE;
	if (h == (iconv_t)-1) {
		return NULL;
	}
	if (iconv(h, (char**)&in, &inb, &out, &outb) != (size_t)-1) {
		*out = 0;
		iconv_close(h);
		return buf;
	}
	iconv_close(h);
	return NULL;
}

enum {PC_WS, PC_ALPHA, PC_LATIN};

static int
is_cp1251 (const char *str)
{
	char *ptr;
	int max_latin = 0, l = 0, lseg = 0, lword = 0;
	int prev_char = PC_WS, start_type = PC_WS;
	for (ptr = (char*)str; ptr && *ptr; ptr = utf8_find_next_char(ptr, NULL)) {
		unsigned char *p = (unsigned char*)ptr;
		if ((*p == 0xc2 && p[1] >= 0xa0 && p[1] <= 0xbf) ||
			(*p == 0xc3 && p[1] >= 0x80 && p[1] <= 0xbf)) {
			/* iso8859-1 character */
			if (!l) {
				start_type = prev_char;
				if (prev_char == PC_WS) lword++;
				else lseg++;
			}
			prev_char = PC_LATIN;
			l++;
			if (l > max_latin) max_latin = l;
		} else if (*p < 0x80) {
			if (isalpha(*p)) {
				if (l && start_type == PC_WS) {
					lword--;
					lseg++;
				}
				prev_char = PC_ALPHA;
			} else prev_char = PC_WS;
			l = 0;
		} else return 0;
	}
	return (!lseg || max_latin > 3) && (lword > 1 || (lword == 1 && max_latin > 1));
}

static int
is_ascii_string (const char *str)
{
	while (*str) {
		if (*str & 0x80) return 0;
		str++;
	}
	return 1;
}

const char *
auto_recode(const char *utf_str)
{
	char *res;
	static char result_buf[MAX_BUFSIZE];
	if (is_ascii_string (utf_str)) {
		res = (char*)utf_str;
	} else if (utf8_validate (utf_str)) {
		/* UTF-8 string */
		/* Replace em dash with "#$%" sequence */
		const char *utf_src = utf_str;
		char iso_8859_1[MAX_BUFSIZE];
		if (simple_recode (utf_str, "UTF-8", "ISO8859-1", iso_8859_1) &&
				utf8_validate(iso_8859_1))
			utf_src = iso_8859_1;
		/* Now find out, how is UTF-8 string constructed */
		if (is_cp1251 (utf_src)) {
			/* May be ISO8859-1 or CP1251 */
			char tmp[MAX_BUFSIZE];
			res = simple_recode (utf_src, "UTF-8", "ISO8859-1", tmp);
			if (res) res = simple_recode (res, "CP1251", "UTF-8", result_buf);
			if (!res) res = (char*)utf_src;
		} else {
			res = strncpy(result_buf, utf_src, MAX_BUFSIZE);
		}
	} else {
		res = simple_recode (utf_str, "CP1251", "UTF-8", result_buf);
		if (!res) res = (char*)utf_str;
	}
	return res;
}

#if 0
#include <stdlib.h>
int main(int argc, char **argv)
{
	char v[4096];
	while (fgets(v, sizeof(v), stdin)) {
		char *p = strchr(v, 10);
		char out[512];
		if (p) *p = 0;
		p = auto_recode(v);
		printf("%s -> %s -> %s\n", v, p, simple_recode(p, "UTF8", "KOI8-R", out));
	}
	return 0;
}
#endif
