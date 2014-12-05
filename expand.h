#ifndef expand_h
#define expand_h

struct str;
struct scope;
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
	const struct scope *scope);

#endif /* expand_h */
