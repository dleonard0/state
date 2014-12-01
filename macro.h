#ifndef macro_h
#define macro_h

struct macro_list;
struct str;

/**
 * A macro is a destructured string, suitable for use in
 * being expanded into another string. 
 * Essentially, a macro is pre-processed text, organised as a
 * tree of references and literals, ready for later expansion.
 * 
 * For example, "$(a b,c)" is a reference to the list of three
 * macros, ["a", "b", "c"].
 */
typedef struct macro {
	struct macro *next;		// the next part of the macro
	enum {
		MACRO_ATOM,
		MACRO_LITERAL,
		MACRO_REFERENCE
	} type;
	union {
		const char *atom;
		struct str *literal;	// backlashes removed; never NULL
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
 * Constructs a new macro containing a literal string.
 * @param str  the string; ownership of the string is TAKEN by the macro
 * @return a macro that must eventually be released with #macro_free()
 */
macro *macro_new_literal(struct str *str);

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
 * Only top-level literal whitespace is considered (ie refs and
 * atoms are ignored).
 * Sequences of whitespace are considered as a single space and removed
 * during the split. Leading and trailing whitespace is removed.
 *
 * @param m  the macro to split. It will be released as if with #macro_free()
 * @returns a macro_list, that must be released with #macro_list_free()
 */
struct macro_list *macro_split(struct macro *m);

/**
 * Trims literal whitespace off the end of a macro.
 * If the macro is all whitespace, the macro is released and @c NULL
 * is stored.
 * @param mp  a pointer to the macro to trim.
 */
void macro_rtrim(struct macro **mp);

/**
 * Trims literal whitespace off the beginning of a macro.
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
