#include <stdio.h>
#include <stdlib.h>

#include "rule.h"
#include "varscope.h"
#include "read.h"
#include "str.h"
#include "parser.h"
#include "expand.h"

struct rule_parse_ctxt {
	const str *path;
	const struct reader *fr;
	void *fctxt;
	void *rctxt;
	struct rule *rule, **rp;
	struct command **commandp;
	struct varscope *scope;
	unsigned errors;
};

/* Writes a string to stderr */
static void
fput_str(FILE *f, const str *str)
{
	stri i;
	for (i = stri_str(str); stri_more(i); stri_inc(i)) {
		putc(stri_at(i), f);
	}
}

/*------------------------------------------------------------
 * Parser callbacks
 */

static void
rule_cb_error(struct parser *p, unsigned lineno, unsigned utf8col, const char *msg)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);

	fput_str(stderr, rpctxt->path);
	fprintf(stderr, ":%u", lineno);
	if (utf8col) {
		fprintf(stderr, ":%u", utf8col);
	}
	fprintf(stderr, ": error: %s\n", msg);
	rpctxt->errors++;
}

static int
rule_cb_read(struct parser *p, char *dst, unsigned len)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);

	/* Just pass through to the reader we were given */
	return rpctxt->fr->read(rpctxt->rctxt, dst, len);
}

static void
rule_cb_define(struct parser *p, macro *lhs, int defkind, macro *text, unsigned lineno)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);
	struct var *var;
	str *lhs_str, **x;
	struct macro **mp;
	atom varname;

	/* Resolving lhs cannot be delayed :) */
	x = expand_macro(&lhs_str, lhs, rpctxt->scope);
	*x = 0;
	varname = atom_from_str(lhs_str);

	if (!varname) {
		rule_cb_error(p, lineno, 0, "empty variable being defined");
		goto out;
	}

	switch (defkind) {
	case DEFKIND_WEAK: 
		var = varscope_get(rpctxt->scope, varname);
		if (var) {
			/* var was already set; do nothing */
			break;
		}
		/* Fallthrough to regular '=' ... */

	case DEFKIND_DELAYED:
		/* Create a new var with the text macro stored directly */
		var = var_new(VAR_DELAYED);
		var->delayed = text;
		text = 0;
		/* Store; any previous var will be freed */
		varscope_put(rpctxt->scope, varname, var);
		break;

	case DEFKIND_IMMEDIATE:
		/* Expand text into a new immediate var */
		var = var_new(VAR_IMMEDIATE);
		x = expand_macro(&var->immediate, text, rpctxt->scope);
		*x = 0;
		varscope_put(rpctxt->scope, varname, var);
		break;

	case DEFKIND_APPEND:
		/* Either get the existing var, or store an empty one */
		var = varscope_get(rpctxt->scope, varname);
		if (!var) {
			var = var_new(VAR_DELAYED);
			var->delayed = 0;
			varscope_put(rpctxt->scope, varname, var);
		}

		switch (var->type) {
		case VAR_IMMEDIATE:
			/* Appending to an existing immediate var;
			 * Walk to the last next pointer of the string */
			x = &var->immediate;
			while (*x) {
				x = &(*x)->next;
			}
			/* Expand and append the text macro directly */
			x = expand_macro(x, text, rpctxt->scope);
			*x = 0;
			break;
		case VAR_DELAYED:
			/* Walk to the end of the stored macro value */
			mp = &var->delayed; 
			while (*mp) {
				mp = &(*mp)->next;
			}
			/* Just attach the text */
			*mp = text;
			text = 0;
			break;
		}
		break;
	}

out:
	macro_free(text);
	macro_free(lhs);
}

static void
rule_cb_directive(struct parser *p, atom ident, macro *text, unsigned lineno)
{
	// struct rule_parse_ctxt *rpctxt = parser_get_context(p);
	macro_free(text);
}

static int
rule_cb_condition(struct parser *p, int condkind, macro *t1, macro *t2, unsigned lineno)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);
	int ret = -1;
	str *s1, *s2, **x;
	atom varname;

	x = expand_macro(&s1, t1, rpctxt->scope);
	*x = 0;
	x = expand_macro(&s2, t2, rpctxt->scope);
	*x = 0;

	switch (condkind) {
	case CONDKIND_IFDEF:
		varname = atom_from_str(s1);
		ret = (varscope_get(rpctxt->scope, varname) != NULL);
		break;
	case CONDKIND_IFEQ:
		ret = (str_cmp(s1, s2) == 0);
		break;
	}

	str_free(s2);
	str_free(s1);

	return ret;
}

static void
rule_cb_rule(struct parser *p, macro *goal, macro *depends, unsigned lineno)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);
	struct rule *rule;

	rule = malloc(sizeof *rule);
	rule->location.filename = str_dup(rpctxt->path);
	rule->location.lineno = lineno;
	rule->goal.macro = goal;
	rule->goal.str = 0;
	rule->depend.macro = depends;
	rule->depend.prereq = 0;
	rpctxt->commandp = &rule->commands;
	rpctxt->rule = rule;
}

static void
rule_cb_command(struct parser *p, macro *text, unsigned lineno)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);
	struct command *command;

	command = malloc(sizeof *command);
	command->location.filename = str_dup(rpctxt->path);
	command->location.lineno = lineno;
	command->macro = text;
	*rpctxt->commandp = command;
	rpctxt->commandp = &command->next;
}

static void
rule_cb_end_rule(struct parser *p)
{
	struct rule_parse_ctxt *rpctxt = parser_get_context(p);

	*rpctxt->commandp = 0;
	*rpctxt->rp = rpctxt->rule;
	rpctxt->rp = &rpctxt->rule->next;
	rpctxt->rule = 0;
}

static struct parser_cb rule_cb = {
	rule_cb_read,
	rule_cb_define,
	rule_cb_directive,
	rule_cb_condition,
	rule_cb_rule,
	rule_cb_command,
	rule_cb_end_rule,
	rule_cb_error
};

struct rule **
rules_parse(struct rule **rp, const struct str *path, struct varscope *scope,
	    const struct reader *fr, void *fctxt)
{
	struct rule_parse_ctxt rpctxt;

	rpctxt.path = path;
	rpctxt.fr = fr;
	rpctxt.rp = rp;
	rpctxt.fctxt = fctxt;
	rpctxt.rctxt = fr->open(fctxt, path);
	rpctxt.scope = scope;
	rpctxt.errors = 0;

	parse(&rule_cb, &rpctxt);
	rp = rpctxt.rp;
	
	return rp;
}
