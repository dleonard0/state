#include <stdlib.h>

#include "str.h"
#include "atom.h"
#include "prereq.h"

struct context {
	stri i;
	const char *error;
};

static struct prereq * parse_all_list(struct context *ctxt);
static struct prereq * parse_any_list(struct context *ctxt);

static struct prereq *
prereq_new(enum prereq_type type)
{
	struct prereq *p = malloc(sizeof *p);
	p->type = type;
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

	skipwhite(ctxt);
	if (canconsume(ctxt, "!")) {
		struct prereq *not = parse_term(ctxt);
		p = prereq_new(PR_NOT);
		p->not = not;
		return p;
	}

	if (canconsume(ctxt, "(")) {
		p = parse_all_list(ctxt);
		if (!canconsume(ctxt, ")")) {
			if (!ctxt->error) {
				ctxt->error = "missing closing )";
			}
		}
		return p;
	}

	if (canconsume(ctxt, "{")) {
		p = parse_any_list(ctxt);
		if (!canconsume(ctxt, "}")) {
			if (!ctxt->error) {
				ctxt->error = "missing closing }";
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
parse_all_list(struct context *ctxt)
{
	struct prereq *ret = 0, **pp = &ret;
	for (;;) {
		skipwhite(ctxt);
		if (!stri_more(ctxt->i))
			break;
		if (couldconsume(ctxt, ")}"))
			break;
		*pp = prereq_new(PR_ALL);
		(*pp)->all.left = parse_term(ctxt);
		pp = &(*pp)->all.right;
	}
	*pp = prereq_new(PR_TRUE);
	return ret;
}

static struct prereq *
parse_any_list(struct context *ctxt)
{
	struct prereq *ret = prereq_new(PR_FALSE);
	struct prereq *p;
	for (;;) {
		skipwhite(ctxt);
		if (!stri_more(ctxt->i))
			break;
		if (couldconsume(ctxt, ")}"))
			break;
		p = prereq_new(PR_ANY);
		p->any.left = ret;
		p->any.right = parse_term(ctxt);
		ret = p;
	}
	return ret;
}

struct prereq *
prereq_make(const struct str *str, const char **error)
{
	struct context ctxt;
	struct prereq *p;
	ctxt.i = stri_str(str);
	ctxt.error = 0;

	p = parse_all_list(&ctxt);

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
	if (p) {
		switch (p->type) {
		case PR_STATE:
			str_free(p->state);
			break;
		case PR_ANY:
			prereq_free(p->any.left);
			prereq_free(p->any.right);
			break;
		case PR_ALL:
			prereq_free(p->all.left);
			prereq_free(p->all.right);
			break;
		case PR_NOT:
			prereq_free(p->not);
			break;
		case PR_TRUE:
			break;
		case PR_FALSE:
			break;
		}
		free(p);
	}
}
