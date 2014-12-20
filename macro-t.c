#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atom.h"
#include "str.h"
#include "macro.h"

/* Macro unit tests */

/**
 * Stringifies a macro into a char buffer.
 * No bounds-checking!
 *
 * @param m   the macro to stringify
 * @param buf where to store the stringified macro
 *
 * @returns pointer to the NUL byte written at the end of the buffer.
 */
char *
macro_to_s(const macro *m, char *buf)
{
	stri i;
	const struct macro_list *l;
	char sep;
	const char *a;

	while (m) {
		switch (m->type) {
		case MACRO_ATOM:
			a = m->atom;
			while (*a)
				*buf++ = *a++;
			break;
		case MACRO_STR:
			for (i = stri_str(m->str);
				stri_more(i); stri_inc(i))
			{
				char ch = stri_at(i);
				if (ch == '$')
					*buf++ = '$';
				*buf++ = ch;
			}
			break;
		case MACRO_REFERENCE:
			l = m->reference;
			sep = ' ';
			*buf++ = '$';
			*buf++ = '(';
			while (l) {
				buf = macro_to_s(l->macro, buf);
				l = l->next;
				if (l)
					*buf++ = sep;
				sep = ',';
			}
			*buf++ = ')';
			break;
		}
		m = m->next;
	}
	*buf = '\0';
	return buf;
}

/**
 * Tests if the macro stringifies to the given string.
 * Prints an error message if the strings differ.
 *
 * @param m  the macro to strigify
 * @param s  the string to compare against the stringification of @a m
 *
 * @return 0 iff the strings differ.
 */
static int
macro_eq(const macro *m, const char *s)
{
	char buf[1024];

	macro_to_s(m, buf);
	if (strcmp(buf, s) == 0) {
		return 1;
	}
	fprintf(stderr, "(expected '%s' but got '%s')\n", s, buf);
	return 0;
}

int
main(void)
{

	STR A = str_new("A");
	STR B = str_new("B");
	STR C = str_new("C");

	{
		/* macro_cons, macro_new_str */
		macro *m = 0;
		macro **acc;

		assert(macro_eq(0, ""));

		acc = &m;
		acc = macro_cons(acc, macro_new_atom(atom_s("xyz")));
		assert(macro_eq(m, "xyz"));
		macro_free(m); m = 0;

		acc = &m;
		acc = macro_cons(acc, macro_new_str(str_dup(A)));
		acc = macro_cons(acc, macro_new_str(str_dup(B)));
		acc = macro_cons(acc, macro_new_str(str_dup(C)));
		assert(macro_eq(m, "ABC"));
		macro_free(m); m = 0;
	}

	{
		/* macro_rtrim, macro_ltrim */
		macro *m = 0;
		macro **acc = 0;

		macro_ltrim(&m);
		assert(!m);
		macro_rtrim(&m);
		assert(!m);

		acc = &m;
		acc = macro_cons(acc, macro_new_str(str_new(" ")));
		acc = macro_cons(acc, macro_new_str(str_new("  x  ")));
		acc = macro_cons(acc, macro_new_str(str_new("    ")));
		assert(macro_eq(m, "   x      "));
		macro_rtrim(&m);
		assert(macro_eq(m, "   x"));
		macro_ltrim(&m);
		assert(macro_eq(m, "x"));
		macro_free(m); m = 0;
	}

	{
		/* macro_split */
		struct macro_list *l = macro_split(
			 macro_new_str(str_new("  this  is a test  ")));

		assert(l);
		assert(macro_eq(l->macro, "this"));
		assert(macro_eq(l->next->macro, "is"));
		assert(macro_eq(l->next->next->macro, "a"));
		assert(macro_eq(l->next->next->next->macro, "test"));
		assert(!l->next->next->next->next);
		macro_list_free(l);

		assert(!macro_split(0));
		assert(!macro_split(macro_new_str(str_new(" "))));
	}

	return 0;
}
