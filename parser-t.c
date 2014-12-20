#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "parser.h"

/* Unit tests for the parser */

/**
 * A capturing context for tests. The plan is to run the
 * parser, and save its callbacks as text appended to the result[] array.
 * Then, we compare an expected string against the actual result[].
 */
struct cb_context {
	const char *input;
	char result[8192];
	char *resultp;
};

/*------------------------------------------------------------
 * cb_context.result[] appending
 */

/** Appends a single char to the context's result[] array */
static void
cb_putc(struct cb_context *context, char c)
{
	assert(context->resultp < &context->result[sizeof context->result - 1]);
	*context->resultp++ = c;
	*context->resultp = '\0';
}

/** Appends a string to the context's result[] array */
static void
cb_puts(struct cb_context *context, const char *text)
{
	while (*text)
		cb_putc(context, *text++);
}

/** Appends a str to the context's result[] array */
static void
cb_putstr(struct cb_context *context, const str *s)
{
	stri i;
	for (i = stri_str(s); stri_more(i); stri_inc(i)) {
		int ch = stri_at(i);
		if (ch == '$')
			cb_putc(context, '$');
		cb_putc(context, ch);
	}
}

/** Appends a macro to the context's result[] array */
static void
cb_putm(struct cb_context *context, const macro *m)
{
	while (m) {
		switch (m->type) {
		case MACRO_ATOM:
			cb_puts(context, m->atom);
			break;
		case MACRO_STR:
			cb_putstr(context, m->str);
			break;
		case MACRO_REFERENCE:
			cb_puts(context, "$(");
			if (m->reference->macro) {
			    char sep = 0;
			    struct macro_list *l = m->reference;
			    while (l) {
				if (sep == 0) sep = ' ';
				else { cb_putc(context, sep); sep = ','; }
				cb_putm(context, l->macro);
				l = l->next;
			    }
			}
			cb_putc(context, ')');
			break;
		}
		m = m->next;
	}
}

/** Appends an integer to the callback's result[] array */
static void
cb_puti(struct cb_context *context, unsigned i)
{
	if (i == 0) {
		cb_putc(context, '0');
	} else {
		if (i > 10) cb_puti(context, i / 10);
		cb_putc(context, '0' + (i % 10));
	}
}

/*------------------------------------------------------------
 * callbacks
 */

/** Reads data recorded inthe cb_context, for the parser */
static int
cb_read(struct parser *p, char *dst, unsigned len)
{
	struct cb_context *context = parser_get_context(p);
	int count;

	if (!context->input)
		return -1;
	count = 0;
	while (len && *context->input) {
		*dst++ = *context->input++;
		len--;
		count++;
	}
	return count;
}

/** Appends "DEFINE var macro\n" to the cb_context.result[]. */
static void
cb_define(struct parser *p, macro *lhs, int defkind, macro *text,
	  unsigned lineno)
{
	struct cb_context *context = parser_get_context(p);

	cb_puts(context, "DEFINE ");
	cb_putm(context, lhs);
	switch (defkind) {
	case DEFKIND_DELAYED:	cb_puts(context, " = "); break;
	case DEFKIND_IMMEDIATE:	cb_puts(context, " := "); break;
	case DEFKIND_WEAK:	cb_puts(context, " ?= "); break;
	case DEFKIND_APPEND:	cb_puts(context, " += "); break;
	default:		cb_puts(context, " <UNKNOWN>= "); break;
	}
	cb_putm(context, text);
	cb_putc(context, '\n');
	macro_free(lhs);
	macro_free(text);
}

/** Appends ".ident macro\n" to the cb_context.result[]. */
static void
cb_directive(struct parser *p, atom ident, macro *text, unsigned lineno)
{
	struct cb_context *context = parser_get_context(p);

	cb_putc(context, '.');
	cb_puts(context, ident);
	if (text) {
		cb_putc(context, ' ');
		cb_putm(context, text);
	}
	cb_putc(context, '\n');
	macro_free(text);
}

/** Appends "IFDEF/IFEQ ...\n" to the cb_context.result[]. */
static int
cb_condition(struct parser *p, int condkind, macro *t1, macro *t2,
	     unsigned lineno)
{
	struct cb_context *context = parser_get_context(p);
	switch (condkind) {
	case CONDKIND_IFDEF:
		cb_puts(context, "IFDEF ");
		cb_putm(context, t1);
		cb_putc(context, '\n');
		assert(!t2);
		break;

	case CONDKIND_IFEQ:
		cb_puts(context, "IFEQ (");
		cb_putm(context, t1);
		cb_putc(context, ',');
		cb_putm(context, t2);
		cb_puts(context, ")\n");
		break;
	default:
		cb_puts(context, "IF <UNKNOWN>\n");
		break;

	}
	macro_free(t1);
	macro_free(t2);
	return 0;
}

/** Appends "RULE goal : depends\n" to the cb_context.result[]. */
static void
cb_rule(struct parser *p, macro *goal, macro *depends, unsigned lineno)
{
	struct cb_context *context = parser_get_context(p);

	cb_puts(context, "RULE ");
	cb_putm(context, goal);
	cb_puts(context, " : ");
	cb_putm(context, depends);
	cb_putc(context, '\n');
	macro_free(goal);
	macro_free(depends);
}

/** Appends "\tCOMMAND-TEXT\n" to the cb_context.result[]. */
static void
cb_command(struct parser *p, macro *text, unsigned lineno)
{
	struct cb_context *context = parser_get_context(p);
	cb_putc(context, '\t');
	cb_putm(context, text);
	cb_putc(context, '\n');
	macro_free(text);
}

/** Appends "ERROR <line>:<col> MSG\n" to the cb_context.result[]. */
static void
cb_error(struct parser *p, unsigned lineno, unsigned u8col, const char *msg)
{
	struct cb_context *context = parser_get_context(p);
	cb_puts(context, "ERROR ");
	cb_puti(context, lineno);
	cb_putc(context, ':');
	cb_puti(context, u8col);
	cb_putc(context, ' ');
	cb_puts(context, msg);
	cb_putc(context, '\n');
}

/** Appends "\n" to the cb_context.result[]. */
static void
cb_end_rule(struct parser *p)
{
	struct cb_context *context = parser_get_context(p);
	cb_putc(context, '\n');
}

static const struct parser_cb test_cb = {
	.read = cb_read,
	.define = cb_define,
	.directive = cb_directive,
	.condition = cb_condition,
	.rule = cb_rule,
	.command = cb_command,
	.error = cb_error,
	.end_rule = cb_end_rule
};

/*------------------------------------------------------------
 * comparison/debugging
 */

/**
 * Prints the str using stdio, but using ANSI colours to
 * make the string more understandable.
 * This is just to make debug easier.
 *
 *   dim fg  - line number
 *   red bg  - hidden whitespace at end of line
 *   green   - visible whitespace (eg, ␉)
 */
static void
fprint_str(FILE *f, const char *str)
{
	unsigned col = 0;
	unsigned line = 0;
	for (; *str; ++str) {
	    int ch = *str;

	    if (col == 0) {
	        line++;
	        fprintf(stderr, "\033[90m%3d:\033[m", line);
	    }

	    /* is this a trailing space */
	    int tsp = 0;
	    if (ch != '\n' && isspace(ch)) {
	        const char *t = str;
		for (tsp = 1; tsp && *t && *t != '\n'; t++) {
		    tsp = isspace(*t);
		}
	    }

	    if (tsp) fputs("\033[41m", f);

	    if (ch == '\t') {
		fputs("\033[32m\xe2\x90\x89", f);
		if ((col & 7) != 7) putc('\t', f);
	    } else {
	        putc(ch, f);
	    }

	    if (tsp || ch == '\t') fputs("\033[m", f);

	    if (ch == '\n') {
	        col = 0;
	    } else if (ch == '\t') {
	        col = (col + 8) & ~7;
	    } else {
		col++;
	    }
	}
	if (col)
	    fputs("\033[31m(no newline at end)\033[m", stderr);
}

/**
 * Parses the input source and compares the stringified result[]
 * with the test case's expected string.
 * On error, prints a detailed message.
 *
 * @param src    an input source string to parse
 * @param expect the C string representing the stringified parse result
 *
 * @return 0 if the stringified parse result differs from expected.
 */
static int
parses_to(const char *src, const char *expect)
{
	struct cb_context ctx;
	int ret;

	ctx.input = src;
	ctx.resultp = ctx.result;
	ctx.result[0] = '\0';
	ret = parse(&test_cb, &ctx);
	if (strcmp(expect, ctx.result) != 0 || ret) {
	    fprintf(stderr,  "\nUsing input: [\n");
	    fprint_str(stderr, src);
	    fprintf(stderr, "]\nExpected output: [\n");
	    fprint_str(stderr, expect);
	    fprintf(stderr, "]\nBut got actual output: [\n");
	    fprint_str(stderr, ctx.result);
	    fprintf(stderr, "]\nWith return code: %d\n", ret);
	    return 0;
	}
	return 1;
}

int
main(void)
{

	/* trivial case */
	assert(parses_to(
	    ""
	    ,
	    ""
	));

	/* blank lines of various kinds */
	assert(parses_to(
	   "# comments, blank lines\n"
	   " #\n"
	   " ##\n"
	   "\n"
	   " \f \r\n"
	   ,
	   ""
	));

	/* various kinds of define, with comments and whatnot */
	assert(parses_to(
	   "X=1\n"
	   "Y=\n"
	   "X := \n"
	   "X :=     # comment\n"
	   "X := a\n"
	   "X := b c   # comment\n"
	   "X += x\n"
	   "X ?= x\n"
	   "Y =# comment\n"
	   ,
	   "DEFINE X = 1\n"
	   "DEFINE Y = \n"
	   "DEFINE X := \n"
	   "DEFINE X := \n"
	   "DEFINE X := a\n"
	   "DEFINE X := b c\n"
	   "DEFINE X += x\n"
	   "DEFINE X ?= x\n"
	   "DEFINE Y = \n"
	));

	/* simple directives */
	assert(parses_to(
	   ".dir\n"
	   ".dir  with some \t text\n"
	   ".dir  ignore #blah\n"
	   ".dir  #blah\n"
	   ,
	   ".dir\n"
	   ".dir with some \t text\n"
	   ".dir ignore \n"
	   ".dir\n"
	));

	/* conditionals */
	assert(parses_to(
	   "ifdef a\n"
	   "X=0\n"   /* this should be hidden */
	   "endif # ignore\n"
	   "ifndef  b   # foo\n"
	   "X=1\n"   /* this should be passed through */
	   "endif \n"
	   "ifeq (a,b)\n"
	   "X=2\n"   /* this should be hidden */
	   "endif\n"
	   "ifneq   (c,d) \n"
	   "X=3\n"   /* this should be passed through */
	   "endif\n"
	   ,
	   "IFDEF a\n"
	   "IFDEF b\n"
	   "DEFINE X = 1\n"
	   "IFEQ (a,b)\n"
	   "IFEQ (c,d)\n"
	   "DEFINE X = 3\n"
	));

	/* nested conditionals, if-else logic */
	assert(parses_to(
	   "ifdef a\n"      /* F             */
	     "X=0\n"        /* F             */
	     "ifdef b\n"    /* F             */
	       "X=1\n"      /* F ?           */
	     "else\n"       /* F             */
	       "X=2\n"      /* F ?           */
	     "endif\n"      /* F             */
	     "X=3\n"        /* F             */
	   "else\n"         /*               */
	     "X=4\n"        /* T             */
	     "ifdef c\n"    /* T             */
	       "X=5\n"      /* T F           */
	     "else\n"       /* T             */
	       "X=6\n"      /* T T           */
	     "endif\n"      /* T             */
	     "X=7\n"        /* T             */
	   "endif\n"        /*               */
	   "X=8\n"          /*               */
	   ,
	   "IFDEF a\n"
	   "DEFINE X = 4\n"
	   "IFDEF c\n"
	   "DEFINE X = 6\n"
	   "DEFINE X = 7\n"
	   "DEFINE X = 8\n"
	));

	/* rules */
	assert(parses_to(
	   "a:\n"
	   " a \t :   #comment\n"
	   " a b c\\:  :\n"
	   "b: c\n"
	   "x:y z  q\n"
	   "a:;foo\n"
	   "a:b;foo\n"
	   ,
	   "RULE a : \n\n"
	   "RULE a : \n\n"
	   "RULE a b c\\: : \n\n"
	   "RULE b : c\n\n"
	   "RULE x : y z  q\n\n"
	   "RULE a : \n\tfoo\n\n" /* sneaky commands */
	   "RULE a : b\n\tfoo\n\n"
	));

	/* macros */
	assert(parses_to(
	   "A = ${B} $*\n"
	   "A = foo${B}bar\n"
	   "A = ${${B}}\n"
	   "A = ${x y,z,q}\n"
	   ,
	   "DEFINE A = $(B) $(*)\n"
	   "DEFINE A = foo$(B)bar\n"
	   "DEFINE A = $($(B))\n"
	   "DEFINE A = $(x y,z,q)\n"
	));

	/* define */
	assert(parses_to(
	   "define E\n"
	   "endef\n"
	   "define A\n"
	   "  foo\n"
	   "  bar\n"
	   "endef\n"
	   "define N\n"
	   " define M\n"
	   "  foo\n"
	   " endef\n"
	   "endef\n"
	   ,
	   "DEFINE E = \n"
	   "DEFINE A = foo\n"
	   "  bar\n"
	   "DEFINE N = define M\n  foo\n endef\n"
	));

	return 0;
}
