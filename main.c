#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pr.h"
#include "str.h"
#include "atom.h"
#include "prereq.h"
#include "varscope.h"
#include "rule.h"
#include "read.h"
#include "globs.h"
#include "expand.h"

/**
 * Applies the entry string "VAR=value" to a varscope
 * @returns 1 if the entry value looked valid
 */
static int
add_var(struct varscope *scope, const char *entry)
{
	const char *equals = entry ? strchr(entry, '=') : 0;
	if (equals) {
		atom name = atom_sn(entry, equals - entry);
		struct var *var = var_new(VAR_IMMEDIATE);
		var->immediate = str_new(equals + 1);
		varscope_put(scope, name, var);
		return 1;
	} else {
		return 0;
	}
}

/** Adds process environment variables to the scope */
static void
add_environ_vars(struct varscope *scope)
{
	extern char **environ;
	char **e;

	for (e = environ; *e; e++) {
		add_var(scope, *e);
	}
}

static struct rule **
load_rules_file(struct rule **rp, const char *filename,
		struct varscope *scope, unsigned *errors)
{
	str *path = str_new(filename);
	FILE *f;

	pr_debug("loading rules from %s", filename);
	f = fopen(filename, "r");
	if (!f) {
		pr_error("%s: %s", filename, strerror(errno));
		++*errors;
	} else {
		rp = rules_parse(rp, path, scope, &stdio_reader, f);
		fclose(f);
	}
	return rp;
}

/* macro that expands to a temporary buffer for use with %.*s formats */
#define FMT_STR ".*s"
#define STR_C(s) str_copy(s, tmp_dst, 0, sizeof tmp_dst), tmp_dst
static char tmp_dst[2048];

/**
 * Tries to satisfy the goals by executing rules.
 * @returns 0 on failure
 */
static int
state(struct globs *globs, struct prereq *goals, struct varscope *scope)
{
	
}

int
main(int argc, char *argv[])
{
	unsigned error = 0;
	int ch;
	struct prereq *args_prereq;
	struct varscope *scope;
	struct rule *rule, *rules, **rp = &rules;
	int files_loaded = 0;
	struct globs *globs = 0;
	int reached;

	/* Create and populate the initial scope */
	scope = varscope_new(0);
	add_environ_vars(scope);

	/* Collect option switches */
	while ((ch = getopt(argc, argv, "vf:")) != -1) {
	    switch (ch) {
	    case 'v':
	    	if (verbosity < V_DEBUG)
			verbosity++;
		break;
	    case 'f':
	    	rp = load_rules_file(rp, optarg, scope, &error);
		files_loaded++;
		break;
	    default: error = 1;
	    }
	}

	if (error) {
		fprintf(stderr, "usage: %s"
				" [-v]"
				" [-f rulefile]"
				" [goal ...]"
				"\n",
			argv[0]
		);
		exit(1);
	}

	if (!files_loaded) {
		rp = load_rules_file(rp, "Staterules", scope, &error);
	}
	*rp = 0; /* terminate rules list */

	/* Pick out any VAR=value args, and
	 * concatenate the rest into one prereq string */
	struct str *args_str, **x = &args_str;
	struct str *space = str_new(" ");
	for (; optind < argc; optind++) {
		if (!add_var(scope, argv[optind])) {
			x = str_xcat(x, space);
			x = str_xcats(x, argv[optind]);
		}
	}
	*x = 0;
	str_ltrim(&args_str);
	str_free(space);

	/* Convert the arg string into a prereq tree */
	if (args_str) {
		const char *prereq_error = 0;
		args_prereq = prereq_make(args_str, &prereq_error);
		str_free(args_str);
		if (!args_prereq && prereq_error) {
			pr_error("argument error: %s", prereq_error);
			error = 1;
		}
	} else {
		/* TODO: what is the default goal? */
		pr_error("no goal specified");
		error = 1;
	}

	/* Build a globset to match all the rule goals */
	globs = globs_new();
	for (rule = rules; rule; rule = rule->next) {
		const char *errmsg;
		if (!rule->goal.str) {
		    x = &rule->goal.str;
		    x = expand_macro(x, rule->goal.macro, scope);
		    *x = 0;
		}
		errmsg = globs_add(globs, rule->goal.str, rule);
		if (errmsg) {
			prl_error(&rule->location, "%s", errmsg);
			error = 1;
		}
	}

	reached = state(globs, args_prereq, scope);

	globs_free(globs);
	rules_free(&rules);
	prereq_free(args_prereq);
	varscope_free(scope);

	exit(reached ? 0 : 1);
}
