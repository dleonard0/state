#include <stdlib.h>

#include "atom.h"
#include "str.h"
#include "scope.h"
#include "macro.h"
#include "dict.h"
#include "expand.h"

/*
 * $(subst FROM,TO,TEXT)
 * Replace all occurrences of FROM in TEXT, with TO.
 * If FROM is empty string, then simply append TO to the end of TEXT
 */
static str **
func_substr(str **x, unsigned argc, const str **args, const struct scope *scope)
{
	const str *FROM = args[1];
	const str *TO = args[2];
	const str *TEXT = args[3];

	if (!FROM) {
		return str_xcat(str_xcat(x, TEXT), TO);
	}

	stri text = stri_str(TEXT);
	stri start = text;
	while (stri_more(text)) {
		stri m = stri_str(FROM);
		stri t = text;
	        while (stri_more(t) && stri_at(t) == stri_at(m)) {
			stri_inc(t);
			stri_inc(m);
			if (!stri_more(m)) {
				x = str_xcatr(x, start, t);
				x = str_xcat(x, TO);
				start = text = t;
				break;
			}
		}
		if (stri_more(m)) {
			stri_inc(text);
		}
	}
	return str_xcatr(x, start, text);
}

typedef str **(*func_t)(str **x, unsigned argc, const str **args, 
			const struct scope *scope);

static struct dict *Func_dict;

static func_t
find_func(atom name)
{
	if (!Func_dict) {
		struct dict *dict = dict_new(0, 0, 0);
#               define add_func(name, func) do { 			\
		    func_t const _f = (func); /* typecheck */ 		\
		    dict_put(dict, atom_s(name), _f); 			\
		} while (0)

		add_func("substr", func_substr);
		Func_dict = dict;
	}
	return dict_get(Func_dict, name);
}


/**
 * @param argc number of arguments
 * @param args all arguments (including a string copy of arg0)
 */
static str **
expand_apply(str **x, atom arg0, unsigned argc, const str **args, const struct scope *scope)
{
	if (argc > 1) {
		func_t func = find_func(arg0);
		if (func) {
			return (*func)(x, argc, args, scope);
		}
	}
	return expand_macro(x, scope_get(scope, arg0), scope);
}

str **
expand_macro(str **x, const macro *macro, const struct scope *scope)
{
	for (; macro; macro = macro->next) {
		switch (macro->type) {
		case MACRO_ATOM:
			x = atom_xstr(x, macro->atom);
			break;
		case MACRO_LITERAL:
			x = str_xcat(x, macro->literal);
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

