#include <stdlib.h>

#include "atom.h"
#include "str.h"
#include "varscope.h"
#include "var.h"
#include "macro.h"
#include "dict.h"
#include "expand.h"

/**
 * A function type for all the "$(func ...)" implementations.
 *
 * @param x      address where to store the start of a generated string
 * @param argc   length of the args[] array
 * @param args   argument strings, "$(func args[0],args[1],...)"
 * @param scope  the current variable scope
 *
 * @return the address for the caller to append further strings,
 *         or for the caaller to store a @c NULL pointer there to
 *         terminate the string.
 */
typedef str **(*func_t)(str **x, unsigned argc, const str **args,
			const struct varscope *scope);

/**
 * Implents the "$(subst FROM,TO,TEXT)" macro expansion.
 * Replace all occurrences of FROM in TEXT, with TO.
 * If FROM is empty string, then simply append TO to the end of TEXT
 *
 * @see #func_t
 */
static str **
func_subst(str **x, unsigned argc, const str **args,
	   const struct varscope *scope)
{
	const str *FROM = args[1];
	const str *TO = args[2];
	const str *TEXT = args[3];

	if (!FROM) {
		return str_xcat(str_xcat(x, TEXT), TO);
	}

	/*
	 * This loop maintains a 'hold' range which is the range of
	 * text that we have tentatively already matched against the
	 * FROM text, but that we anticipate 'emitting' should the tentative
	 * match fail at an unwanted character..
	 * Because we don't want to return single-character
	 * strings at a time, we also maintain an 'out' range, so
	 * that a compact string is constructed.
	 */
	stri text;	/* current match point of loop */
	stri out_start;	/* start of the hold range */
	stri out_end;	/* end of the hold range */

	out_start = out_end = text = stri_str(TEXT);
	while (stri_more(text)) {
		stri f = stri_str(FROM);
		stri t = text;
		/* Try to match FROM at position t */
	        while (stri_more(t) && stri_at(t) == stri_at(f)) {
			stri_inc(t);
			stri_inc(f);
			if (!stri_more(f)) {
				/* got a full match of FROM in TEXT! */
				x = str_xcatr(x, out_start, out_end);
				x = str_xcat(x, TO);
				text = f;	/* skip ahead in TEXT */
				out_start = out_end = text; /* restart */
				break;
			}
		}
		if (stri_more(f)) { /* (no match) */
			stri_inc(text);
			out_end = text;
		}
	}
	/* Copy out the straggler hold range */
	return str_xcatr(x, out_start, out_end);
}

/** A $(func) dictionary mapping "func" atoms to #func_t pointers,
 *  used to speed up subsequent lookups. */
static struct dict *Func_dict;

/** Cleans up #Func_dict */
static void
find_func_atexit()
{
	dict_free(Func_dict);
}

/**
 * Find the function implementation pointer for the named function.
 *
 * @param  name  the function's name
 *
 * @return a pointer to the function, or
 *         @c NULL if the function is unknown
 */
static func_t
find_func(atom name)
{
	if (!Func_dict) {
		struct dict *dict = dict_new(0, 0, 0);
#               define add_func(name, func) do {			\
		    func_t const _f = (func); /* typecheck */		\
		    dict_put(dict, atom_s(name), _f);			\
		} while (0)

		add_func("subst", func_subst);
		/* TODO: add more functions here */

		Func_dict = dict;
		atexit(find_func_atexit);
	}
	return dict_get(Func_dict, name);
}


/**
 * Expands a destructured variable/function "$(arg0 arg1,arg2,...)"
 * attaching the resulting text to the given attachment point.
 *
 * @param x     attachment point for generated strings
 * @param arg0  name of the variable or function to expand
 * @param argc  number of arguments
 * @param args  all arguments (including a string copy of arg0)
 * @param scope the current variable scope
 *
 * @return next string attachment point
 */
static str **
expand_apply(str **x, atom arg0, unsigned argc, const str **args,
	     const struct varscope *scope)
{
	if (argc > 1) {
		func_t func = find_func(arg0);
		if (func) {
			return (*func)(x, argc, args, scope);
		}
	}
	return expand_var(x, varscope_get(scope, arg0), scope);
}

str **
expand_var(str **x, const struct var *var, const struct varscope *scope)
{
	if (var) {
		switch (var->type) {
		case VAR_IMMEDIATE:
			x = str_xcat(x, var->immediate);
			break;
		case VAR_DELAYED:
			x = expand_macro(x, var->delayed, scope);
			break;
		}
	}
	return x;
}

str **
expand_macro(str **x, const macro *macro, const struct varscope *scope)
{
	for (; macro; macro = macro->next) {
		switch (macro->type) {
		case MACRO_ATOM:
			x = atom_xstr(x, macro->atom);
			break;
		case MACRO_STR:
			x = str_xcat(x, macro->str);
			break;
		case MACRO_REFERENCE:
			{
				const struct macro_list *ml;
				str **args;
				unsigned argc;

				/* count the number of references */
				for (ml = macro->reference, argc = 0;
				     ml; ml = ml->next)
				{
					++argc;
				}
				if (argc == 0) {
					/* $() expands to nothing */
					continue;
				}

				/* Initialize the args to be at least 10 long,
				 * and default to empty */
				args = calloc(argc < 10 ? 10 : argc,
					      sizeof *args);

				/* recurively expand available args to strs */
				for (ml = macro->reference, argc = 0;
				     ml; ml = ml->next)
				{
					str **ax = &args[argc++];
					ax = expand_macro(ax, ml->macro, scope);
					*ax = 0;
				}

				/* convert first arg to an atom */
				atom arg0 = atom_from_str(args[0]);

				x = expand_apply(x, arg0, argc,
					(const str **)args, scope);

				/* We deallocate the strings */
				while (argc--) {
					str_free(args[argc]);
				}
				free(args);
				break;
			}
			break;
		}
	}
	return x;
}

