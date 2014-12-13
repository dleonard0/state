#include <stdlib.h>

#include "str.h"
#include "atom.h"
#include "prereq.h"

struct context {
	stri i;
	const char *error;
};

static struct prereq * parse_list(struct context *ctxt);

static struct prereq *
prereq_new(enum prereq_type type)
{
	struct prereq *p = malloc(sizeof *p);
	p->type = type;
	p->next = 0;
	return p;
}

static unsigned
couldconsume(struct context *ctxt, const char *chs)
{
	if (stri_more(ctxt->i)) {
		unsigned ch = stri_at(ctxt->i);
		for (; *chs; ++chs) {
			if (ch == *chs) {
				return ch;
			}
		}
	}
	return 0;
}

static unsigned
canconsume(struct context *ctxt, const char *chs)
{
	unsigned ch = couldconsume(ctxt, chs);
	if (ch) {
		stri_inc(ctxt->i);
	}
	return ch;
}

static stri
find(struct context *ctxt, const char *chs)
{
	const char *s;
	stri i = ctxt->i;

	while (stri_more(i)) {
		unsigned ch = stri_at(i);
		for (s = chs; *s; s++) {
			if (ch == *s) {
				return i;
			}
		}
		stri_inc(i);
	}
	return i;
}

static void skipwhite(struct context *ctxt) {
	while (canconsume(ctxt, " \t"))
		;
}

static struct prereq *
parse_term(struct context *ctxt)
{
	struct prereq *p;
	unsigned ch;

	skipwhite(ctxt);
	if (canconsume(ctxt, "!")) {
		struct prereq *not = parse_term(ctxt);
		if (not && not->type == PR_NOT) {
			/* double negative */
			p = not->not;
			not->not = 0;
			prereq_free(not);
			return p;
		}
		p = prereq_new(PR_NOT);
		p->not = not;
		return p;
	}

	if ((ch = canconsume(ctxt, "({"))) {
		struct prereq *list = parse_list(ctxt);
		const char * closech;
		if (ch == '(') {
			p = prereq_new(PR_ALL);
			p->all = list;
			closech = ")";
		} else {
			p = prereq_new(PR_ANY);
			p->any = list;
			closech = "}";
		}
		if (!canconsume(ctxt, closech)) {
			if (!ctxt->error) {
				ctxt->error = "missing closing ) or }";
			}
		}
		return p;
	}

	stri end = find(ctxt, " \t(){}");
	str *state, **x;
	x = str_xcatr(&state, ctxt->i, end);
	*x = 0;

	if (!state && !ctxt->error)
		ctxt->error = "missing state";
	/* TODO: check for '@' */

	p = prereq_new(PR_STATE);
	p->state = state;
	ctxt->i = end;

	return p;
}

static struct prereq *
parse_list(struct context *ctxt)
{
	struct prereq *ret, **lastp = &ret;

	for (;;) {
		skipwhite(ctxt);
		if (!stri_more(ctxt->i))
			break;
		if (couldconsume(ctxt, ")}"))
			break;
		struct prereq *p = parse_term(ctxt);
		*lastp = p;
		lastp = &p->next;
	}
	*lastp = 0;
	return ret;
}

struct prereq *
prereq_make(const struct str *str, const char **error)
{
	struct context ctxt;
	struct prereq *p;
	ctxt.i = stri_str(str);
	ctxt.error = 0;

	p = prereq_new(PR_ALL);
	p->all = parse_list(&ctxt);

	if (!ctxt.error) {
		/* check for extraneous characters */
		skipwhite(&ctxt);
		if (stri_more(ctxt.i)) {
			ctxt.error = "unexpected characters";
		}
	}

	if (ctxt.error) {
		*error = ctxt.error;
		prereq_free(p);
		p = 0;
	}
	return p;
}

void
prereq_free(struct prereq *p)
{
	struct prereq *next = p;
	while ((p = next)) {
		switch (p->type) {
		case PR_STATE:
			str_free(p->state);
			break;
		case PR_ANY:
			prereq_free(p->any);
			break;
		case PR_ALL:
			prereq_free(p->any);
			break;
		case PR_NOT:
			prereq_free(p->any);
			break;
		}
		next = p->next;
		free(p);
	}
}
