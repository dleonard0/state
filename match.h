#ifndef match_h
#define match_h

#include "str.h"	/* stri */

/**
 * A matcher object coordinates the output of a string #generator with
 * the selective filtering of a #glob pattern set. The intended use of this is
 * to lazily match glob patterns against paths in a filesystem.
 *
 * The string generators are allowed to produce incomplete strings (called
 * "deferreds") which are common prefixes for future strings. When a
 * deferred string is matched, the generator is asked to generate more strings.
 *
 * Conceptually, the generators provide a lazily evaluated string set,
 * and the matcher is an incremental rejection algorithm.
 *
 * For example, to match the pattern 'h*.txt' against files in the
 * current working directory, a generator could produce the initial
 * list of files from the current directory, plus 'deferreds' for the
 * subdirectories (and the root directory). The generated set could be:
 *
 *      0          0     0       0           0
 *    [ ^hello.txt ^ha.c ^subdir ^subdir/... ^/... ]
 *
 * Where the caret ^ indicates the match candidature of the string, the
 * trailing ... means a 'defer point', and the digit 0 indicates
 * the caret state relative to a glob set's DFA.
 *
 * On the first iteration the matcher steps the pattern h*.txt over all
 * the strings. A pattern iterator is an automaton state, one is created
 * for each generated string. They are each stepped according to where
 * the caret is.
 *                  ╭┬───┬───┬───╮
 *                  ↓?   ?   ?   ?
 *             →○─h→○┴.→○┴t→○┴x→○┴t→●
 *              0   1   2   3   4   5
 *
 * The pattern automaton in its initial state will only transition on 'h',
 * and any other charater will be 'rejected'.  Rejection is used by the
 * matcher to discard immediately candidate strings.
 *
 * After stepping each automaton (patterni) by one character for each
 * element in the generated list, elements can be removed. The list in our
 * example then reduces to:
 *       1          1
 *    [ h^ello.txt h^a.c ]
 *
 * The next step, roughly corresponding to the wildcard '*', only moves
 * the caret through '?' (any char) edges.
 *        1          1
 *    [ he^llo.txt ha^.c ]
 *
 *         1          2
 *    [ hel^lo.txt ha.^c ]
 *
 *          1          1
 *    [ hell^o.txt ha.c^ ]
 *
 * On the next step, the ha.c element will be at the end of its string, so
 * it is checked to see in an accepting state. Only state 5 is an accepting
 * state in the DFA. So, ha.c is rejected and removed from the list.
 *
 * Had the string ha.c been marked "deferred", its element would be replaced
 * with those strings from another call to the generator. That could have
 * happend had ha.c been a directory pathname.
 *
 * The filtering continues until an element reaches end-of-string:
 *
 *            2
 *    [ hello.^txt ]
 *             3
 *    [ hello.t^xt ]
 *               4
 *    [ hello.tx^t ]
 *               5
 *    [ hello.txt^ ]
 *
 * A undeferred member string successfully reaching an accept state is then
 * removed from the list, and returned as a result from #matcher_next().
 */
struct matcher;

struct globs; /* forward decl */

/**
 * A match is a partially-matched candidate string.
 * These structures are allocated by the generator callback implementation.
 */
struct match {
	struct match *next;
	str *str;		/**< candidate string, UTF-8 encoded (owned) */
	unsigned flags;
#define MATCH_DEFERRED	1	/**< flags: generator can yield more strings */
	stri stri;		/**< position of next character to match */
	unsigned state;		/**< current match state */
};

/**
 * Allocates a new match structure.
 * Only the #match.flags and #match.str fields are initialized.
 *
 * @param str  the UTF-8 string in the match (TAKEN)
 *
 * @returns a new match structure, partially initialized.
 */
struct match *match_new(str *str);

/**
 * Deallocates a match structure.
 *
 * @param match the match structure to release
 */
void          match_free(struct match *match);

/**
 * A callback interface that provides candidate strings to the #matcher.
 */
struct generator {
	/**
	 * Provides a list of candidate strings to the matcher. This
	 * is the main callback interface.
	 *
	 * This function should allocate #match structures with #match_new(),
	 * chain them via their #match.next fields, and insert them
	 * into the list indicated by the @a mp parameter.
	 *
	 * Each returned match must have a #match.str field that is:
	 *   1. longer then @a prefix
	 *   2. starts with the same characters as @a prefix
	 *
	 * The #match.flags field must be set to 0 or MATCH_DEFERRED.
	 * The #match.stri field may be left uninitialized.
	 * The #match.state field may be left uninitialized.
	 * The last #match.next field may be left uninitialized.
	 * Any other fields should be initialized to zero.
	 *
	 * The MATCH_DEFERRED bit in #match.flags indicates that this
	 * function should be called again to provided more strings.
	 * Such a call will have #match.str passed as the @a prefix
	 * parameter.
	 *
	 * @param mp      address of a #match.next field into which to
	 *                store the new #match elements
	 * @param prefix  The UTF-8 string whose extension is being deferred,
	 *                or the empty string (@c NULL).
	 * @param gcontext The generator context argument given to
	 *                #matcher_new().
	 *
	 * @return address of last #match.next field or @a mp
	 */
	struct match ** (*generate)(struct match **mp,
				    const str *prefix,
				    void *gcontext);
	/**
	 * Releases the context pointer passed to #matcher_new().
	 * This is called by #matcher_free().
	 * This function pointer may be left as @c NULL.
	 *
	 * @param gcontext The generator context argument given to
	 *                #matcher_new().
	 */
	void (*free)(void *gcontext);
};


/**
 * Allocates a new matcher, which can be used to filter a generated
 * string space.
 *
 * @param globs      a compiled set of glob patterns.
 * @param generator  interface for obtaining new candidate strings
 * @param gcontext   context pointer for the generator
 *
 * @returns the new matcher
 */
struct matcher *matcher_new(const struct globs *globs,
			    const struct generator *generator,
			    void *gcontext);

/**
 * Searches the generated string space to find the string that matches
 * a pattern from the globset.
 *
 * @param matcher     The matcher that is currently matching.
 * @param ref_return  Optional pointer to storage where to
 *                    store the glob's associated reference.
 *
 * @returns the UTF-8 str that matched (caller must free this string),
 *          or @c NULL when the generator is exhausted.
 */
str *matcher_next(struct matcher *matcher, const void **ref_return);

/**
 * Releases storage associated with a matcher.
 *
 * @param matcher The matcher to release.
 */
void matcher_free(struct matcher *matcher);

#endif /* match_h */
