#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cclass.h"

/**
 * Converts a unicode code point into a canonical, printable representation.
 *
 * @param p   pointer into storage that will hold the representation
 *            (storage boundary is unchecked)
 * @param ch  the unicode character to represent
 *
 * @return pointer into the storage position immediately after the
 *         representation stored at @a p.
 */
static char *
ch_tostr(char *p, unsigned ch)
{
#define HEX(ch,nib)  hex[((ch) >> (4*(nib))) & 0xf]
	const char hex[] = "0123456789ABCDEF";
	if      (ch == 0)    { *p++ = '\\'; *p++ = '0'; }
	else if (ch == '-'
	      || ch == '\\'
	      || ch == ']')  { *p++ = '\\'; *p++ = ch; }
	else if (ch == '\n') { *p++ = '\\'; *p++ = 'n'; }
	else if (ch == '\r') { *p++ = '\\'; *p++ = 'r'; }
	else if (ch == '\t') { *p++ = '\\'; *p++ = 't'; }
	else if (ch < 0x20)  { *p++ = '\\'; *p++ = 'x';
			       *p++ = hex[(ch >> 4) & 0xf];
			       *p++ = hex[(ch >> 0) & 0xf]; }
	else if (ch < 0x7f)  { *p++ = ch; }
	else if (ch <=0xffff){ *p++ = '\\'; *p++ = 'u';
			       *p++ = hex[(ch >> 12) & 0xf];
			       *p++ = hex[(ch >>  8) & 0xf];
			       *p++ = hex[(ch >>  4) & 0xf];
			       *p++ = hex[(ch >>  0) & 0xf]; }
	else                 { *p++ = '\\'; *p++ = 'u';  *p++ = '+';
			       *p++ = hex[(ch >> 20) & 0xf];
			       *p++ = hex[(ch >> 16) & 0xf];
			       *p++ = hex[(ch >> 12) & 0xf];
			       *p++ = hex[(ch >>  8) & 0xf];
			       *p++ = hex[(ch >>  4) & 0xf];
			       *p++ = hex[(ch >>  0) & 0xf]; }
	return p;
}

/**
 * Converts a cclass into a canonical representation.
 *
 * @param cc  the cclass to stringify.
 *
 * @returns pointer to a C string in temporary static storage.
 */
static const char *
cclass_tostr(const cclass *cc)
{
	static char buf[1024];
	char *p = buf;
	unsigned i;
	unsigned lasthi = 0;

	if (!cc) return "NULL";

	for (i = 0; i < cc->nintervals; ++i) {
	    unsigned lo = cc->interval[i].lo;
	    unsigned hi = cc->interval[i].hi;
	    assert(p < &buf[sizeof buf - sizeof "\\u+000000-\\u+000000]"]);
	    if (i && lasthi >= lo) {
		memcpy(p, "*OVERLAP*", 9); p += 9;
	    }
	    p = ch_tostr(p, lo);
	    if (hi > lo + 2) *p++ = '-';
	    if (hi != MAXCHAR && hi > lo + 1) p = ch_tostr(p, hi - 1);
	    lasthi = hi;
	}
	*p = '\0';
	return buf;
}

/**
 * Tests if the cclass has the expected canonical representation.
 * If it fails, an error message is printed out, and the function
 * returns false.
 *
 * @param cc       the cclass to check
 * @param expected the epected string representation
 *
 * @returns true if @a cc stringifies to the same string as @a expected.
 */
static int
cclass_eqstr(const cclass *cc, const char *expected)
{
	const char *cc_str = cclass_tostr(cc);
	if (strcmp(cc_str, expected) == 0) {
		return 1;
	} else {
		fprintf(stderr, "expected %s but got %s\n", expected, cc_str);
		return 0;
	}
}

/** Converts a number into a hexadecimal digit */
static unsigned hexval(char c) { return c <= '9' ? c - '9' :
					c <= 'F' ? c - 'A' + 10 :
					           c - 'a' + 10; }

/**
 * Parses the string at s to consume a encoding of a unicode character.
 * The encoding of @a s must be ASCII, with the following backlash escapes
 * understood:
 *
 *     \0 \n \r \t \\ \xXX \uXXXX \u+XXXXXX
 *
 * @param s   pointer to a string index; the pointer will be advanced.
 *
 * @returns the code point of the unicode character parsed.
 */
static unsigned
parse_char(const char **s)
{
	unsigned digs, r = 0;
	char ch = *(*s)++;
	if (ch == '\\' && **s) {
		ch = *(*s)++;
		if (ch == '0') return '\0';
		if (ch == 'n') return '\n';
		if (ch == 'r') return '\r';
		if (ch == 't') return '\t';
		if (ch == 'x' || ch == 'u') {
		    if (ch == 'x') digs = 2;
		    else { digs = 4; if (**s == '+')  digs = 6, (*s)++; }
		    while (digs-- && **s)
			 r = hexval(*(*s)++) | r << 4;
		    return r;
		}
	}
	return ch;
}

/**
 * Makes a cclass from a string repr.
 *   For example, "ac-y" -> {[a,b),[c,y)}.
 * This is a convenience function for the unit tests.
 * It does not understand inverses.
 * It does not require square brackets.
 * To get a hyphen, use a leading hypen.
 */
static cclass *
make_cclass(const char *str)
{
	const char *s = str;
	unsigned lo, hi;
	cclass *cc = cclass_new();

	while (*s) {
		lo = parse_char(&s);
		if (*s == '-') {
			if (!*++s) hi = MAXCHAR;
			else       hi = parse_char(&s) + 1;
		} else  {
			hi = lo + 1;
		}
		cclass_add(cc, lo, hi);
	}
	return cc;
}

int
main()
{
	{
		/* empty cclass */
		cclass *cc = cclass_new();
		assert(cclass_is_empty(cc));
		assert(!cclass_is_single(cc));
		assert(!cclass_contains(cc, 'a', 'z'));
		assert(!cclass_contains_ch(cc, 'a'));
		assert(cclass_eq(cc, cc));
		cclass_free(cc);
	}
	{
		/* singleton cclass */
		cclass *cc = make_cclass("b");
		assert(cclass_eqstr(cc, "b"));
		assert(!cclass_is_empty(cc));
		assert(cclass_is_single(cc));
		assert(!cclass_contains(cc, 'a', 'a'+1));
		assert(cclass_contains(cc, 'b', 'b'+1));
		assert(!cclass_contains(cc, 'c', 'c'+1));
		assert(!cclass_contains_ch(cc, 'a'));
		assert(cclass_contains_ch(cc, 'b'));
		assert(!cclass_contains_ch(cc, 'c'));
		assert(cclass_eq(cc, cc));
		cclass_free(cc);
	}
	{
		/* single-character adding and containment */
		cclass *cc = cclass_new();
		cclass_add(cc, 'a', 'a' + 1);	/* [a] */
		cclass_add(cc, 'b', 'b' + 1);	/* [ab] */

		assert(cc->nintervals == 1);
		assert(cclass_eqstr(cc, "ab"));

		assert(!cclass_is_empty(cc));
		assert(!cclass_is_single(cc));
		assert(cclass_contains(cc, 'a', 'a'+1));
		assert(cclass_contains(cc, 'b', 'b'+1));
		assert(cclass_contains(cc, 'a', 'b'+1));
		assert(cclass_contains_ch(cc, 'a'));
		assert(cclass_contains_ch(cc, 'b'));
		assert(!cclass_contains_ch(cc, 'c'));
		cclass_free(cc);
	}
	{
		/* Reversed adding */
		cclass *cc = cclass_new();
		cclass_add(cc, 'b', 'b'+1);	/* [b] */
		cclass_add(cc, 'a', 'a'+1);	/* [ab] */
		assert(cclass_contains(cc, 'a', 'b'+1));

		assert(cclass_eqstr(cc, "ab"));
		cclass_free(cc);
	}
	{
		/* non-single adding and splitting */
		cclass *c1 = make_cclass("a-cm-px-z");
		cclass *c2;

		assert(cclass_eqstr(c1, "a-cm-px-z"));
		c2 = cclass_split(c1, 'n');	/* [a-cm] [n-px-z] */
		assert(cclass_eqstr(c1, "a-cm"));
		assert(cclass_eqstr(c2, "n-px-z"));

		assert(cclass_contains(c1, 'a', 'c'+1));
		assert(cclass_contains_ch(c1, 'm'));
		assert(!cclass_contains_ch(c1, 'n'));
		assert(!cclass_contains_ch(c1, 'x'));

		assert(!cclass_contains_ch(c2, 'a'));
		assert(!cclass_contains_ch(c2, 'm'));
		assert(cclass_contains(c2, 'n', 'p'+1));
		assert(cclass_contains(c2, 'x', 'z'+1));

		cclass_free(c1);
		cclass_free(c2);
	}
	{
		/* inverses */
		cclass *c1;

		c1 = cclass_invert(cclass_new());
		assert(cclass_eqstr(c1, "\\0-"));	/* [0,MAX) */
		cclass_invert(c1);
		assert(cclass_is_empty(c1));
		cclass_free(c1);

		c1 = cclass_invert(make_cclass("\\0-a"));
		assert(cclass_eqstr(c1, "b-"));
		cclass_invert(c1);
		assert(cclass_eqstr(c1, "\\0-a"));
		cclass_free(c1);

		c1 = cclass_invert(make_cclass("\\0-ap-s"));
		assert(cclass_eqstr(c1, "b-ot-"));
		cclass_invert(c1);
		assert(cclass_eqstr(c1, "\\0-ap-s"));
		cclass_free(c1);

		c1 = cclass_invert(make_cclass("\\0-ap-sx-"));
		assert(cclass_eqstr(c1, "b-ot-w"));
		cclass_invert(c1);
		assert(cclass_eqstr(c1, "\\0-ap-sx-"));
		cclass_free(c1);

		c1 = cclass_invert(make_cclass("x-"));
		assert(cclass_eqstr(c1, "\\0-w"));
		cclass_invert(c1);
		assert(cclass_eqstr(c1, "x-"));
		cclass_free(c1);

	}
	{
		/* cclass_addcc */
		cclass *cc = cclass_new();
		cclass *mp = make_cclass("m-p");
		cclass *af = make_cclass("a-f");
		cclass *g = make_cclass("g");
		cclass *di = make_cclass("d-i");
		cclass *su = make_cclass("s-uy");
		cclass *sy = make_cclass("s-y");
		cclass *bt = make_cclass("b-t");

		cclass_addcc(cc, mp);
		assert(cclass_eqstr(cc, "m-p"));
		cclass_addcc(cc, mp);
		assert(cclass_eqstr(cc, "m-p"));

		cclass_addcc(cc, af);
		assert(cclass_eqstr(cc, "a-fm-p"));
		cclass_addcc(cc, af);
		assert(cclass_eqstr(cc, "a-fm-p"));

		cclass_addcc(cc, g);
		assert(cclass_eqstr(cc, "a-gm-p"));
		cclass_addcc(cc, g);
		assert(cclass_eqstr(cc, "a-gm-p"));

		cclass_addcc(cc, di);
		assert(cclass_eqstr(cc, "a-im-p"));
		cclass_addcc(cc, di);
		assert(cclass_eqstr(cc, "a-im-p"));

		cclass_addcc(cc, su);
		assert(cclass_eqstr(cc, "a-im-ps-uy"));
		cclass_addcc(cc, su);
		assert(cclass_eqstr(cc, "a-im-ps-uy"));

		cclass_addcc(cc, sy);
		assert(cclass_eqstr(cc, "a-im-ps-y"));
		cclass_addcc(cc, sy);
		assert(cclass_eqstr(cc, "a-im-ps-y"));

		cclass_addcc(cc, bt);
		assert(cclass_eqstr(cc, "a-y"));
		cclass_addcc(cc, bt);
		assert(cclass_eqstr(cc, "a-y"));

		cclass_free(bt);
		cclass_free(sy);
		cclass_free(su);
		cclass_free(di);
		cclass_free(g);
		cclass_free(af);
		cclass_free(mp);
		cclass_free(cc);
	}
	{
		/* cclass_contains_cc */
		cclass *c1 = make_cclass("a-ch-mp-t");
		cclass *a;

		assert(cclass_contains_cc(c1, c1));

		a = make_cclass("a-c");
		assert(cclass_contains_cc(c1, a));
		assert(!cclass_contains_cc(a, c1));
		cclass_free(a);

		a = make_cclass("a-h");
		assert(!cclass_contains_cc(c1, a));
		assert(!cclass_contains_cc(a, c1));
		cclass_free(a);

		a = make_cclass("mp");
		assert(cclass_contains_cc(c1, a));
		assert(!cclass_contains_cc(a, c1));
		cclass_free(a);

		a = make_cclass("t");
		assert(cclass_contains_cc(c1, a));
		assert(!cclass_contains_cc(a, c1));
		cclass_free(a);

		a = make_cclass("t-u");
		assert(!cclass_contains_cc(c1, a));
		assert(!cclass_contains_cc(a, c1));
		cclass_free(a);

		cclass_free(c1);
	}
	/*
	 *  [a,b),[x,y)   -> [0,a),[b,x),[y,MAX)
	 *  [0,b),[x,y)   ->       [b,x),[y,MAX)
	 *  [a,b),[x,MAX) -> [0,a),[b,x)
	 *  [0,MAX)       -> empty
	 *  empty         -> [0,MAX)
	 */


	return 0;
}
