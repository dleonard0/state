#ifndef expand_h
#define expand_h

struct str;
struct scope;
struct macro;

/*
 * Expands a macro into a string.
 * The references in the macro are recursively expanded
 * using the scope, and known make-like functions are also
 * implemented.
 *
 * @param str_ret  Where to store the start of the string resulting 
 *		   from expanding the macro.
 * @param macro    The macro to expand into a string.
 * @param scope    Variable scope.
 * @returns address of the last (uninitialized) #str.next pointer in
 *          the string, or @a str_ret parameter. See #str_xcat().
 */
struct str **expand_macro(struct str**str_ret, const struct macro *macro, 
	const struct scope *scope);

#endif /* expand_h */
