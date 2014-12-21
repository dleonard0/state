#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "str.h"
#include "var.h"
#include "varscope.h"
#include "dict.h"
#include "parser.h"
#include "expand.h"

/*------------------------------------------------------------
 * Let's begin with a test framework for expanding macros.
 *
 * This is a minimalist parser callback provider, so we can
 * use #parse() to turn test strings into macro, store them in
 * variables, and then expand those macros back into strings.
 *
 * The idea is to construct an in-memory 'file' of the form:
 *
 *     VAR1 = <value>
 *     VAR2 = <value>
 *     .macro <macro-beging-tested>
 *
 * When parsing this completes, we will have received the test macro
 * through the (fake) .macro directive, and also collected variables
 * into a scope definition along the way. The macro is simply
 * expanded using the expand function (which is really what is being
 * tested here) and the result compared against an expected C string.
 *
 * Some effort has been expended to in displaying descrepancies
 * between the input and the resulting macro structure. This was to
 * assist in fixing bugs.
 */

/** My macro test parse context */
struct mm {
	const char *file;	  /**< set to __FILE__ of #assert_expands() */
	int lineno;		  /**< set to __LINE__ of #assert_expands() */
	const char * const *text; /**< the faked file input */
	unsigned texti;		  /**< next index into text[] */
	const char *textp;	  /**< current char pos in text[texti-1] */
	macro *macro;		  /**< receives value from .macro callback */
	struct varscope *scope;	  /**< receives variable definitions */
};

static int
mm_read(struct parser *p, char *dst, unsigned len)
{
	struct mm *mm = parser_get_context(p);
	int rlen = 0;

	while (len && mm->textp) {
		if (*mm->textp) {
			*dst++ = *mm->textp++;
			len--;
			rlen++;
		} else {
			mm->textp = mm->text[mm->texti++];
		}
	}
	return rlen;
}

static void
mm_directive(struct parser *p, atom ident, macro *text, unsigned lineno)
{
	struct mm *mm = parser_get_context(p);
	assert(!mm->macro);
	mm->macro = text;
}

static void
mm_define(struct parser *p, macro *lhs, int defkind, macro *text,
	  unsigned lineno)
{
	struct mm *mm = parser_get_context(p);

	/* only support defines of the form NAME=... for testing */
	assert(defkind == DEFKIND_DELAYED);
	assert(lhs && lhs->type == MACRO_STR && !lhs->next);

	struct var *var = var_new(VAR_DELAYED);
	var->delayed = text;
	varscope_put(mm->scope, atom_from_str(lhs->str), var);
	macro_free(lhs);
}

static void
mm_error(struct parser *p, unsigned lineno, unsigned u8col, const char *msg)
{
	struct mm *mm = parser_get_context(p);
	fprintf(stderr, "%s:%d: parse error at (%u,%u): %s\n",
		mm->file, mm->lineno, lineno, u8col, msg);

	fprintf(stderr, "parser text was:\n");
	unsigned tline = 0;
	unsigned tcol = 0;
	unsigned texti = 0;
	const char *textp = mm->text[0];
	while (textp) {
		if (*textp) {
			if (tcol == 0) {
				tline++;
				fprintf(stderr, "\033[90m%4d:\033[m ", tline);
			}
			char ch = *textp++;
			int hilite = (lineno == tline && tcol == u8col);
			if (hilite) {
				fprintf(stderr, "\033[31;1;7m");
			} else if (ch == '\n' || ch == '\t') {
				hilite = 1;
				fprintf(stderr, "\033[90m");
			}
			putc(ch == '\n' ? '$' :
			     ch == '\t' ? '>' :
			     ch, stderr);
			if (hilite) {
				fprintf(stderr, "\033[m");
			}
			if (ch == '\n') {
				putc('\n', stderr);
				tcol = 0;
			} else {
				tcol++;
			}
		} else {
			textp = mm->text[++texti];
		}
	}

	abort();
}

/** Prints a unicode character in an unambiguous way, for debug.
 *  The unicode character is printed in ASCII, using backslash escapes
 *  to represent non-ASCII, unprintable ASCII, and the following
 *  special characters (which don't normally appear in macros references):
 *
 *	<newline> \ ' $ ) ,
 */
static void
print_char(FILE *f, int ch)
{
	if (ch == '\n')
		fprintf(f, "\\n");
	else if (ch == '\\' || ch == '\'' ||
		 ch == '$' || ch == ')' || ch == ',')
		fprintf(f, "\\%c", ch);
	else if (ch < ' ')
		fprintf(f, "\\x%02x", ch);
	else if (ch < 0x7f)
		putc(ch, f);
	else if (ch <= 0xffff)
		fprintf(f, "\\u%04x", ch);
	else
		fprintf(f, "\\u+%06x", ch);
}

/** Prints an atom, for debug */
static void
print_atom(FILE *f, atom a)
{
	const char *s;
	for (s = a; *s; ++s) {
		print_char(f, *s);
	}
}

/** Prints a str, for debug. The string is assumed to be encoded in UTF-8 */
static void
print_str(FILE *f, str *s)
{
	stri i;
	for (i = stri_str(s); stri_more(i); ) {
		print_char(f, stri_utf8_inc(&i));
	}
}

/**
 * Prints a macro, using ANSI colour highlighting to indicate structure.
 *    plain:     string part
 *    fg yellow: atom part
 *    fg blue:   reference metachars: "$(", "," and ")"
 *    bg red:    corrupt structure
 */
static void
print_macro(FILE *f, macro *m)
{
	struct macro_list *ml;
	for (; m; m = m->next) {
		switch (m->type) {
		case MACRO_ATOM:
			fprintf(f, "\033[33m");
			print_atom(f, m->atom);
			fprintf(f, "\033[m");
			break;
		case MACRO_STR:
			print_str(f, m->str);
			break;
		case MACRO_REFERENCE:
			fprintf(f, "\033[34m$(\033[m");
			ml = m->reference;
			if (ml) {
				print_macro(f, ml->macro);
				char sep = ' ';
				for (ml = ml->next; ml; ml = ml->next) {
					fprintf(f, "\033[34m%c\033[m", sep);
					sep = ',';
					print_macro(f, ml->macro);
				}
			}
			fprintf(f, "\033[34m)\033[m");
			break;
		default:
			fprintf(f, "\033[41m*ERROR*\033[m");
			break;
		}
	}
}

/**
 * Asserts that the macro text expands into an expected string.
 *
 * @param text      the text to be parsed as a macro then expanded
 * @param expected  the expected string value after expanding the parsed macro
 * @param ...       optional "VAR=value\n" definitions for the var scope
 */
#define assert_expands(text, expected, ...) \
	assert_expands_(__FILE__, __LINE__, expected, text, "" __VA_ARGS__)
static void
assert_expands_(const char *file, int lineno,
	const char *expected, const char *text, const char *defines)
{
	struct parser_cb cb = {
		.read = mm_read,
		.define = mm_define,
		.directive = mm_directive,
		.error = mm_error,
	};
	const char * const texts[] = { ".macro ", text, "\n", defines, "\n", 0 };
	struct mm mm = {
		.file = file,
		.lineno = lineno,
		.text = texts,
		.texti = 0, .textp = "",
		.macro = 0,
		.scope = varscope_new(0),
	};

	parse(&cb, &mm);

	str *actual, **x = &actual;
	x = expand_macro(x, mm.macro, mm.scope);
	*x = 0;

	if (!str_eq(actual, expected)) {
		fprintf(stderr, "%s:%d: unexpected macro expansion\n"
		                "  expected '",
			mm.file, mm.lineno);
		print_atom(stderr, expected); /* not really an atom, but ok */
		fprintf(stderr, "'\n  actual   '");
		print_str(stderr, actual);
		fprintf(stderr, "'\n  macro    '");
		print_macro(stderr, mm.macro);
		fprintf(stderr, "'\n");

		struct dict_iter *di = dict_iter_new(mm.scope->scope.dict);
		const void *key;
		void *value;
		fprintf(stderr, "  scope content:\n");
		while (dict_iter_next(di, &key, &value)) {
			fprintf(stderr, "    %-4s = '", (const char *)key);
			print_macro(stderr, (macro *)value);
			fprintf(stderr, "'\n");
		}
		dict_iter_free(di);
		fflush(stderr);
		exit(1);
	}

	str_free(actual);
	macro_free(mm.macro);
	varscope_free(mm.scope);
}


int
main()
{
	/* basic expasion tests */
	assert_expands("", "");
	assert_expands("abc", "abc");
	assert_expands("ab$(X)c", "abc");		/* undefined $(X) */
	assert_expands("ab$()c", "abc");
	assert_expands("ab$( )c", "abc");
	assert_expands("a$(X)c", "abc", "X = b");
	assert_expands("a$(X)c", "abc", "X = $(Y)\nY = b");
	assert_expands("a$($(X))c", "abc", "X = Y\nY = b");

	/* subst */
	assert_expands("a$(subst fofobar,M,$(X))c",	"afofofoMc",
		       "X = fofofofofobar");
	assert_expands("a$(subst ,b,x)c",		"axbc");

	return 0;
}
