#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "parser.h"

/*
 * This is a capturing context. The plan is to run the 
 * parser, and to save its callbacks as text into the result[] array.
 * Then, we compare an expected string against the output result[].
 * What follows are callback functions that append to the result[] array.
 */
struct cb_context {
	const char *input;
	char result[8192];
	char *resultp;
};

/* appends a char to the result[] array */
static void
cb_putc(struct cb_context *context, char c)
{
	assert(context->resultp < &context->result[sizeof context->result - 1]);
	*context->resultp++ = c;
	*context->resultp = '\0';
}

/* appends a string to the result[] array */
static void
cb_puts(struct cb_context *context, const char *text)
{
	while (*text)
		cb_putc(context, *text++);
}

/* appends a str to the result[] array */
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

/* appends a macro to the result[] array */
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

/* appends an integer to the result[] array */
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


static void
cb_define(struct parser *p, macro *lhs, int defkind, macro *text)
{
	struct cb_context *context = parser_get_context(p);

	cb_puts(context, "DEFINE ");
	cb_putm(context, lhs);
	switch (defkind) {
	case DEFKIND_DELAYED: 	cb_puts(context, " = "); break;
	case DEFKIND_IMMEDIATE:	cb_puts(context, " := "); break;
	case DEFKIND_WEAK:	cb_puts(context, " ?= "); break;
	case DEFKIND_APPEND:	cb_puts(context, " += "); break;
	default:            	cb_puts(context, " <UNKNOWN>= "); break;
	}
	cb_putm(context, text);
	cb_putc(context, '\n');
	macro_free(lhs);
	macro_free(text);
}

static void
cb_directive(struct parser *p, atom ident, macro *text)
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

static int
cb_condition(struct parser *p, int condkind, macro *t1, macro *t2)
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

static void
cb_rule(struct parser *p, macro *goal, macro *depends)
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

static void
cb_command(struct parser *p, macro *text)
{
	struct cb_context *context = parser_get_context(p);
	cb_putc(context, '\t');
	cb_putm(context, text);
	cb_putc(context, '\n');
	macro_free(text);
}

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

/*
 * Prints the string using stdio, but uses ANSI colours to
 * make the string more understandable. just for debug
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
		fputs("\033[32m\u2409", f);
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

/* parse the input source and compare the result[] against the expect string */
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

	return 0;
}
