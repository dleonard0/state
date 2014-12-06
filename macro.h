#ifndef macro_h
#define macro_h

struct macro_list;
struct str;

/**
 * A macro is a non-recursive tree-structured object, suitable
 * for expansion into a string by using the macro-valued variables
 * held in a variable scope/dictionary.
 *
 * A macro is stored as a single-linked list of macro parts
 * that logically form a tree. Each macro part is either:
 *    atom      - an interned string (see <atom.h>)
 *    str       - a shared, literal string (see <str.h>)
 *    reference - a structured reference denoted $(a b,c,...)
 *                but held in memory as the linked-list {a,b,c,...}
 *
 * References are represented as $(a b,c,...). Notice that
 * the first and second arguments are separated by whitespace,
 * while the subsequent arguments are separated by a single
 * comma character. This is for compatibility with GNU make.
 *
 * Macros are constructed by the parser module, see #parse().
 * They can be "expanded" into a concrete string, see #expand_macro().
 */
typedef struct macro {
	struct macro *next;		// the next part of the macro
	enum {
		MACRO_ATOM,
		MACRO_STR,
		MACRO_REFERENCE
	} type;
	union {
		const char *atom;
		struct str *str;	// backlashes removed; never NULL
		struct macro_list *reference; // $(macro macro,macro,...)
	};
} macro;

/* A simple linked list of macros */
struct macro_list {
	struct macro_list *next;
	macro *macro;
};

/**
 * Constructs a new macro containing an atom.
 * @param atom  the atom
 * @return a macro that must eventually be released with #macro_free()
 */
macro *macro_new_atom(const char *atom);

/**
 * Constructs a new macro containing a non-atomic, literal string.
 * @param str  the string; ownership of the string is TAKEN by the macro
 * @return a macro that must eventually be released with #macro_free()
 */
macro *macro_new_str(struct str *str);

/**
 * Constructs a new reference macro, with no elements; i.e. $()
 * @return a macro that must eventually be released with #macro_free()
 */
macro *macro_new_reference(void);

/**
 * Appends a macro onto the end of another macro.
 * @param mp pointer to the last macro#next pointer of a macro
 * @param m  the macro to append onto the macro ended by @a mp. This
 *           pointer will be TAKEN by the macro
 * @return pointer to the (new) last macro#next pointer in the macro.
 *         The entire resulting macro must eventually be released
 *         using #macro_free().
 */
macro **macro_cons(macro **mp, macro *m);

/**
 * Deallocates a macro
 */
void   macro_free(macro *macro);

/**
 * Appends a macro onto the end of a macro_list
 * @param lp pointer to the last macro_list#next pointer in a macro_list
 * @param m  the macro to append onto the macro_list pointed at by @a lp.
 * @return pointer to last last macro_list#next pointer in the macro_list
 *         chain after appending @a m.
 *         It will always dereference to @c NULL.
 *         The entire resulting macro must eventually be released
 *         using #macro_list_free().
 */
struct macro_list **macro_list_cons(struct macro_list **lp, macro *m);

/**
 * Splits a macro on whitespace.
 * Backslashes before whitespace characters prevent splitting.
 * Only top-level literal string whitespace is considered (ie refs and
 * atoms are ignored).
 * Sequences of whitespace are considered as a single space and removed
 * during the split. Leading and trailing whitespace is removed.
 *
 * @param m  the macro to split. It will be released as if with #macro_free()
 * @returns a macro_list, that must be released with #macro_list_free()
 */
struct macro_list *macro_split(struct macro *m);

/**
 * Trims whitespace off the end of a macro.
 * If the macro is all whitespace, the macro is released and @c NULL
 * is stored.
 * @param mp  a pointer to the macro to trim.
 */
void macro_rtrim(struct macro **mp);

/**
 * Trims whitespace off the beginning of a macro.
 * If the macro is all whitespace, the macro is released and @c NULL
 * is stored.
 * @param mp  a pointer to the macro to trim.
 */
void macro_ltrim(struct macro **mp);

/*
 * Deallocates a macro_list
 */
void   macro_list_free(struct macro_list *macro_list);

#endif /* macro_h */
