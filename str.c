#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "str.h"

asm(".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
    ".byte 1\n"
    ".asciz \"str-gdb.py\"\n"
    ".popsection\n");

int str_count;

/**
 * Allocates an unintialized str component.
 * @return the allocation, never @c NULL.
 */
static str *
str_alloc()
{
	str *s = malloc(sizeof (str));
	str_count++;
	return s;
}

/**
 * Deallocates a destroyed string component,
 * as produced by #str_free().
 */
static void
str_dealloc(str *s)
{
	free(s);
	str_count--;
}

static void
release_seg(struct str_seg *seg)
{
	if (--seg->refs == 0) {
		seg->data[0]='#';
		free(seg);
	}
}

/*
 * A str is a sequence of offset:len references into read-only
 * char[] segments. When no string references a segment, the
 * segment can be released.
 */
str *
str_newn_(const char *data, unsigned len, const char *file, unsigned lineno)
{
	str *str;
	struct str_seg *seg;
	
	if (len == 0) {
		return NULL;
	}

	str = str_alloc();
	str->seg = seg = malloc(sizeof (struct str_seg) + len - 1);
	memcpy(seg->data, data, len);
	seg->refs = 1;
	str->offset = 0;
	str->len = len;
	str->next = NULL;
	return str;
}

str *
str_new_(const char *cstr, const char *file, unsigned lineno)
{
	return str_newn_(cstr, strlen(cstr), file, lineno);
}

str **
str_xcat(str **dst, const str *s)
{
	str **ret = dst;
	for (; s; s = s->next) {
		str *str = str_alloc();

		str->offset = s->offset;
		str->len = s->len;
		str->seg = s->seg;
		str->seg->refs++;

		*ret = str;
		ret = &str->next;
	}
	return ret;
}

str *
str_cat(const str *a, const str *b)
{
	str *str, **x = &str;

	x = str_xcat(x, a);
	x = str_xcat(x, b);
	*x = NULL;
	return str;
}

int
str_cmp(const str *a, const str *b)
{
	stri ai, bi;

	if (a == b) {
		return 0;
	}

	ai = stri_str(a);
	bi = stri_str(b);

	while (stri_more(ai) && stri_more(bi)) {
		/* optimize for when segments are shared */
		if (ai.str->seg == bi.str->seg &&
		    ai.str->offset + ai.pos == bi.str->offset + bi.pos)
		{
		    unsigned minlen = ai.str->len;
		    if (bi.str->len < minlen)
			    minlen = bi.str->len;
		    minlen--;
		    ai.pos += minlen;
		    bi.pos += minlen;
		} else {
		    char ca = stri_at(ai);
		    char cb = stri_at(bi);
		    if (ca < cb)
			return -1;
		    if (ca > cb)
			return 1;
		}
		stri_inc(ai);
		stri_inc(bi);
	}

	if (stri_more(ai))
		return 1;
	if (stri_more(bi))
		return -1;
	return 0;
}

int
str_eq(const str *a, const char *s)
{
	stri ai = stri_str(a);
	while (*s && stri_more(ai)) {
		if (*s != stri_at(ai))
			return 0;
		s++;
		stri_inc(ai);
	}
	return !*s && !stri_more(ai);
}

str *
str_dup(const str *a)
{
	str *ret, **x = &ret;
	x = str_xcat(x, a);
	*x = NULL;
	return ret;
}

void
str_freep(str * const * sp)
{
	str_free(*sp);
}

void
str_free(str *next)
{
	str *s;

	while ((s = next)) {
		next = s->next;
		release_seg(s->seg);
		str_dealloc(s);
	}
}

str *
str_substr(const str *s, unsigned offset, unsigned len)
{
	str *ret, **nextp;

	if (len) {
		while (s && offset >= s->len) {
			offset -= s->len;
			s = s->next;
		}
	}
	nextp = &ret;
	while (len && s) {
		*nextp = str_alloc();
		(*nextp)->seg = s->seg;
		(*nextp)->offset = s->offset + offset;
		(*nextp)->len = s->len - offset;
		if ((*nextp)->len > len)
			(*nextp)->len = len;
		(*nextp)->seg->refs++;
		len -= (*nextp)->len;
		offset = 0;
		nextp = &(*nextp)->next;
		s = s->next;
	}
	*nextp = NULL;
	return ret;
}

unsigned
str_len(const str *str)
{
	unsigned len = 0;

	while (str) {
		len += str->len;
		str = str->next;
	}
	return len;
}

str *
str_tok(stri *i, const char *sep)
{
	stri start;
	unsigned len;

	while (stri_more(*i) && strchr(sep, stri_at(*i)))
		stri_inc(*i);
	start = *i;
	len = 0;
	while (stri_more(*i) && !strchr(sep, stri_at(*i))) {
		len++;
		stri_inc(*i);
	}
	return str_substr(start.str, start.pos, len);
}

char
str_at(const str *s, unsigned pos)
{
	while (s && pos >= s->len) {
		pos -= s->len;
		s = s->next;
	}
	if (!s)
		return '\0';
	return s->seg->data[s->offset + pos];
}

unsigned
str_hash(const str *s)
{
	unsigned h = 0;
	unsigned i;

	for (; s; s = s->next)
		for (i = 0; i < s->len; i++)
			h = (h << 1) ^ s->seg->data[s->offset + i];
	return h;
}

unsigned
str_copy(const str *s, char *dst, unsigned offset, unsigned len)
{
	unsigned count = 0;

	while (s && offset >= s->len) {
		offset -= s->len;
		s = s->next;
	}
	while (s && len) {
		int slen = len + offset <= s->len ? len : s->len - offset;
		memcpy(dst, &s->seg->data[s->offset + offset], slen);
		offset = 0;
		len -= slen;
		dst += slen;
		count += slen;
		s = s->next;
	}
	return count;
}

void
str_pack(const str *fixed, str *s)
{
	if (!fixed)
		return;
	for (; s; s = s->next) {
		const str *f;
		for (f = fixed; f; f = f->next) {
			if (f->seg == s->seg)
				break;
			if (f->len == s->len &&
			    memcmp(&f->seg->data[f->offset],
			           &s->seg->data[s->offset],
				   f->len) == 0)
			{
				release_seg(s->seg);
				s->seg = f->seg;
				s->offset = f->offset;
				s->seg->refs++;
				break;
			}
		}
	}
}

void
str_ltrim(str **sp)
{
        str *s;

        while ((s = *sp)) {
            while (s->len && isspace(s->seg->data[s->offset])) {
                s->offset++;
                s->len--;
            }
            if (s->len)
                break;
            *sp = s->next;
            s->next = 0;
            str_free(s);
        }
}

void
str_rtrim(str **sp)
{
        str *s = *sp;

        if (!s)
		return;
        str_rtrim(&s->next);
	if (s->next)
		return;
        while (s->len && isspace(s->seg->data[s->offset + s->len - 1]))
                s->len--;
        if (s->len)
		return;
        str_free(s);
        *sp = 0;
}

str *
str_split_at(str **sp, unsigned offset)
{
	str *s, *s2;

	while (*sp && offset >= (*sp)->len) {
		offset -= (*sp)->len;
		sp = &(*sp)->next;
	}
	if (!*sp)
		return 0;

	s = *sp;
	if (!offset) {
		*sp = 0;
		return s;
	}

	s2 = str_alloc();

	s2->next = s->next;
	s->next = 0;

	s2->seg = s->seg;
	s->seg->refs++;

	s2->offset = s->offset + offset;
	s2->len = s->len - offset;
	s->len = offset;

	return s2;
}

#define MAKE_UTF8_ERROR(ch) (0xdc80u | (ch))

unsigned
stri_utf8_inc(stri *i)
{
	/*
	 * UTF-8 encoding:
	 * 0xxxxxxx                                                  7f
	 * 110xxxxx 10xxxxxx                                        7ff
	 * 1110xxxx 10xxxxxx 10xxxxxx                              ffff
	 * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx                   1fffff
	 *
	 * Be careful to return erroneous bytes in 0x80..0xff as
	 * 0xdc80..0xdcff so that the original (bad) UTF-8 encoding
	 * can be reconstructed.
	 */
	unsigned ch0 = stri_at(*i) & 0xff;
	stri_inc_by(i, 1);
	if ((ch0 & 0x80) == 0x00) {
		return ch0;
	}

	unsigned want;
	unsigned minvalid;
	if ((ch0 & 0xe0) == 0xc0) {
		ch0 &= 0x1f;
		want = 1;
		minvalid = 0x80;
	} else if ((ch0 & 0xf0) == 0xe0) {
		ch0 &= 0x0f;
		want = 2;
		minvalid = 0x800;
	} else if ((ch0 & 0xf8) == 0xf0) {
		minvalid = 0x10000;
		ch0 &= 0x07;
		want = 3;
	} else {
		return MAKE_UTF8_ERROR(ch0);
	}
	if (!stri_more_by(*i, want)) {
		return MAKE_UTF8_ERROR(ch0);
	}

	stri tmp = *i;
	unsigned ch = ch0;
	while (want--) {
		unsigned nextch = stri_at(tmp) & 0xff;
		stri_inc(tmp);
		if ((nextch & 0xc0) != 0x80) {
			return MAKE_UTF8_ERROR(ch0);
		}
		ch = (ch << 6) | (nextch & 0x3f);
	}

	if (ch < minvalid || (ch & 0xfff800) == 0xd800) {
		return MAKE_UTF8_ERROR(ch0);
	}

	*i = tmp;
	return ch;
}
