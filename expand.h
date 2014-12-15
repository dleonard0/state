#ifndef expand_h
#define expand_h

struct str;
struct varscope;
struct macro;

/*
 * Expands the given macro into a string.
 * $(var) references in the macro are recursively expanded
 * using the provided scope, and some make-like functions are also
 * implemented, eg $(subst ...).
 *
 * @param str_ret  Where to store the start of the string resulting
 *		   from expanding the macro.
 * @param macro    The macro to expand.
 * @param scope    Variable scope to use for references.
 * @returns address of the last (uninitialized) #str.next pointer in
 *          the string, or @a str_ret parameter. See #str_xcat().
 */
struct str **expand_macro(struct str**str_ret, const struct macro *macro,
	const struct varscope *scope);

/*
 * Expands the given var into a string.
 * Similar to #expand_macro().
 *
 * @param str_ret  Where to store the start of the string resulting
 *		   from expanding the macro.
 * @param var      The var to expand, or @c NULL.
 * @param scope    Variable scope to use for references.
 * @returns address of the last (uninitialized) #str.next pointer in
 *          the string, or @a str_ret parameter. See #str_xcat().
 */
struct str **expand_var(struct str**str_ret, const struct var *var, 
	const struct varscope *scope);

#endif /* expand_h */
