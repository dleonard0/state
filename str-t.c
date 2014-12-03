#include <assert.h>
#include <string.h>

#include "str.h"

extern int str_count;

int
main(void)
{
	{
		/* empty strings */
		assert(str_new("") == 0);
		assert(str_len(0) == 0);
		assert(str_at(0, 0) == 0);
	}
	{
		/* str_at */
		STR a = str_new("abc");
		assert(str_len(a) == 3);
		assert(str_at(a, 0) == 'a');
		assert(str_at(a, 1) == 'b');
		assert(str_at(a, 2) == 'c');
	}
	{
		/* str_cat, str_cmp */
		STR a = str_new("a");
		STR b = str_new("b");
		STR c = str_new("c");
		STR a_b = str_cat(a, b);
		STR a_b_c = str_cat(a_b, c);
		STR abc = str_new("abc");

		assert(str_cmp(a, a) == 0);
		assert(str_cmp(a, b) == -1);
		assert(str_cmp(c, b) == 1);
		assert(str_cmp(abc, a_b_c) == 0);
		assert(str_cmp(a_b_c, abc) == 0);
		assert(str_cmp(a, a_b_c) == -1);
		assert(str_cmp(a_b, a_b_c) == -1);
		assert(str_cmp(a_b_c, a) == 1);
		assert(str_cmp(a_b_c, a_b) == 1);
		assert(str_cmp(0, 0) == 0);
		assert(str_cmp(0, a_b) == -1);
		assert(str_cmp(a_b, 0) == 1);
	}
	{
		/* str_dup */
		STR a = str_new("hello");
		STR b = str_new("there");
		STR a_b = str_cat(a, b);
		STR c = str_dup(a_b);
		assert(str_cmp(a_b, c) == 0);
		assert(str_len(c) == 10);
		assert(str_at(a_b, 8) == 'r');
	}
	{
		/* str_substr */
		STR a = str_new("hello");
		STR b = str_new("there");
		STR a_b = str_cat(a, b);

		STR s1 = str_substr(a_b, 2, 3);
		STR llo = str_new("llo");
		assert(str_cmp(s1, llo) == 0);
	}
	{
		/* str_tok */
		STR a = str_new("  this  is\ta    test   ");

		stri i = stri_str(a);
		STR w1 = str_tok(&i, " \t");
		assert(str_eq(w1, "this"));
		STR w2 = str_tok(&i, " \t");
		assert(str_eq(w2, "is"));
		STR w3 = str_tok(&i, " \t");
		assert(str_eq(w3, "a"));
		STR w4 = str_tok(&i, " \t");
		assert(str_eq(w4, "test"));
		STR w5 = str_tok(&i, " \t");
		assert(w5 == 0);

		STR b = str_new("s");
		i = stri_str(b);
		STR v1 = str_tok(&i, " \t");
		assert(str_eq(v1, "s"));
		STR v2 = str_tok(&i, " \t");
		assert(v2 == 0);
	}
	{
		/* stri iterators */
		stri i = stri_str(0);
		assert(!stri_more(i));

		STR a = str_new("a");
		STR bb = str_new("bb");
		STR c = str_new("c");
		STR a_bb = str_cat(a, bb);
		STR a_bb_c = str_cat(a_bb, c);

		i = stri_str(a_bb_c);		/* .abbc */
		assert(stri_more(i));
		assert(stri_at(i) == 'a');
		assert(stri_more_by(i, 4));

		stri_inc_by(&i, 3);		/* abb.c */
		assert(stri_more(i));
		assert(stri_at(i) == 'c');
		assert(stri_more_by(i, 1));
		assert(!stri_more_by(i, 2));

		stri_inc(i);			/* abbc. */
		assert(!stri_more(i));
		assert(!stri_more_by(i, 1));
		assert(!stri_more_by(i, 3));
	}
	{
		/* str_pack */
		STR a1 = str_new("abc");
		STR b1 = str_new("def");
		STR a2 = str_new("xyz");
		STR b2 = str_new("def");
		STR ab1 = str_cat(a1, b1);
		STR ab2 = str_cat(a2, b2);
		str_pack(ab1, ab2);
		assert(str_eq(ab2, "xyzdef"));
	}
	{
		/* str_copy */
		STR a = str_new("abc");
		STR b = str_new("def");
		STR c = str_new("ghi");
		STR a_b = str_cat(a, b);
		STR a_b_c = str_cat(a_b, c);

		char buf[10];
		assert(str_copy(a_b_c, buf, 0, 1) == 1);
		assert(memcmp(buf, "a", 1) == 0);
		assert(str_copy(a_b_c, buf, 0, 6) == 6);
		assert(memcmp(buf, "abcdef", 6) == 0);
		assert(str_copy(a_b_c, buf, 0, 9) == 9);
		assert(memcmp(buf, "abcdefghi", 9) == 0);
		assert(str_copy(a_b_c, buf, 0, 10) == 9);
		assert(memcmp(buf, "abcdefghi", 9) == 0);
		assert(str_copy(a_b_c, buf, 4, 10) == 5);
		assert(memcmp(buf, "efghi", 5) == 0);
	}
	{
		/* str_ltrim, str_rtrim */
		str *a =  str_new("  foo bar bax  ");
		str_rtrim(&a);
		assert(str_eq(a, "  foo bar bax"));
		str_ltrim(&a);
		assert(str_eq(a, "foo bar bax"));

		STR b = str_new("   ");
		STR c = str_new("     ");

		str *b_c = str_cat(b, c);
		str *b_c2 = str_dup(b_c);
		str_rtrim(&b_c);
		assert(!b_c);
		str_ltrim(&b_c2);
		assert(!b_c2);

		str_free(a);
		str_free(b_c);
		str_free(b_c2);
	}
	{
		/* str_split_at */
		str *a = str_new("this is a test");
		str *b = str_split_at(&a, 5);
		assert(str_eq(a, "this "));
		assert(str_eq(b, "is a test"));

		str *c = str_split_at(&b, 0);
		assert(!b);
		assert(str_eq(c, "is a test"));

		str *d = str_split_at(&a, 5);
		assert(str_eq(a, "this "));
		assert(!d);

		str_free(a);
		str_free(b);
		str_free(c);
		str_free(d);
	}
	{
		/* UTF-8 decoding */
		str *eps = str_new("x\xce\xb5");
		stri i = stri_str(eps);
		unsigned ch = stri_utf8_at(i);
		assert(ch == 'x');
		assert(!IS_INVALID_UTF8(ch));
		ch = stri_utf8_inc(&i);
		assert(ch == 'x');
		assert(stri_more(i));
		ch = stri_utf8_inc(&i);
		assert(ch == 0x3b5);
		assert(!IS_INVALID_UTF8(ch));
		assert(!stri_more(i));
		str_free(eps);

		str *bad = str_new("\x80\x82");
		i = stri_str(bad);
		ch = stri_utf8_inc(&i);
		assert(IS_INVALID_UTF8(ch));
		assert((ch & 0xff) == 0x80);
		assert(stri_more(i));
		ch = stri_utf8_inc(&i);
		assert(IS_INVALID_UTF8(ch));
		assert((ch & 0xff) == 0x82);
		assert(!stri_more(i));
		str_free(bad);
	}
	{
		STR word = str_new("word");
		STR hello = str_new("hello");
		str **x;

		str *wordhello;
		x = &wordhello;
		x = str_xcat(x, word);
		x = str_xcat(x, hello);
		*x = NULL;
		assert(str_eq(wordhello, "wordhello"));

		str *s;
		x = &s;
		x = str_xcat(x, wordhello);
		x = str_xcat(x, wordhello);
		*x = NULL;
		assert(str_eq(s, "wordhellowordhello"));

		str_free(wordhello);
		str_free(s);
	}

	assert(str_count == 0);

	return 0;
}
