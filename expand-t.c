#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "str.h"
#include "macro.h"
#include "scope.h"
#include "dict.h"
#include "parser.h"
#include "expand.h"

struct mm {
	const char *file;
	int lineno;
	const char * const *text;
	unsigned texti;    /* next index into text[] */
	const char *textp; /* current char pos in text[texti-1] */
	macro *macro;
	struct scope *scope;
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
			mm->textp = mm->text[++mm->texti];
		}
	}
	return rlen;
}

static void
mm_directive(struct parser *p, atom ident, macro *text)
{
	struct mm *mm = parser_get_context(p);
	assert(!mm->macro);
	mm->macro = text;
}

static void
mm_define(struct parser *p, macro *lhs, int defkind, macro *text)
{
	struct mm *mm = parser_get_context(p);

	// only support defines of the form NAME=... for testing
	assert(defkind == DEFKIND_DELAYED);
	assert(lhs && lhs->type == MACRO_LITERAL && !lhs->next);

	scope_put(mm->scope, atom_from_str(lhs->literal), text);
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
	else if (ch >= 0x7f)
		fprintf(f, "\\u%04x", ch);
	else
		putc(ch, f);
}

static void
print_atom(FILE *f, atom a)
{
	const char *s;
	for (s = a; *s; ++s) {
		print_char(f, *s);
	}
}

static void
print_str(FILE *f, str *s)
{
	stri i;
	for (i = stri_str(s); stri_more(i); stri_inc(i)) {
		print_char(f, stri_at(i));
	}
}

static void
print_macro(FILE *f, macro *m)
{
	struct macro_list *ml;
	for (; m; m = m->next) {
		switch (m->type) {
		case MACRO_ATOM:
			print_atom(f, m->atom);
			break;
		case MACRO_LITERAL:
			print_str(f, m->literal);
			break;
		case MACRO_REFERENCE:
			fprintf(f, "$(");
			ml = m->reference;
			if (ml) {
				print_macro(f, ml->macro);
				char sep = ' ';
				for (ml = ml->next; ml; ml = ml->next) {
					putc(sep, f);
					sep = ',';
					print_macro(f, ml->macro);
				}
			}
			putc(')', f);
			break;
		default:
			fprintf(f, "*ERROR*");
			break;
		}
	}
}

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
	const char * const texts[] = { ".macro ", text, "\n", defines, 0 };
	struct mm mm = {
		.file = file,
		.lineno = lineno,
		.text = texts,
		.texti = 0, .textp = "",
		.macro = 0,
		.scope = scope_new(0, (void (*)(void *))macro_free),
	};

	parse(&cb, &mm);

	str *actual, **x = &actual;
	x = expand_macro(x, mm.macro, mm.scope);
	*x = 0;

	if (!str_eq(actual, expected)) {
		fprintf(stderr, "%s:%d: expected expansion\n  expected '",
			mm.file, mm.lineno);
		print_atom(stderr, expected); // not really an atom, but ok
		fprintf(stderr, "'\n  actual   '");
		print_str(stderr, actual);
		fprintf(stderr, "'\n  macro    '");
		print_macro(stderr, mm.macro);
		fprintf(stderr, "'\n");

		struct dict_iter *di = dict_iter_new(mm.scope->dict);
		const void *key;
		void *value;
		while (dict_iter_next(di, &key, &value)) {
			fprintf(stderr, "  %s = '", (const char *)key);
			print_macro(stderr, (macro *)value);
			fprintf(stderr, "'\n");
		}
		dict_iter_free(di);
		fflush(stderr);
		abort();
	}

	str_free(actual);
	macro_free(mm.macro);
	scope_free(mm.scope);
}

int
main()
{
	assert_expands("", "");
	assert_expands("abc", "abc");
	assert_expands("ab$(nothing)c", "abc");
	return 0;
}
