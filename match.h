#ifndef match_h
#define match_h

/*
 * A matcher object coordinates the output of a string generator with
 * the selective filtering of a pattern.
 *
 * The string generators are allowed to produce incomplete strings (called
 * "deferreds") which are essentially common prefixes for future strings.
 * This permits efficient matching of patterns over a compressed string space.
 *
 * Conceptually, the generators implement a compressed set,
 * and the matcher is an incremental rejection algorithm.
 *
 * For example, to match the pattern 'h*.txt' against files in the 
 * current working directory, the generator would produce the initial
 * list of files in the current directory plus 'deferreds' for the
 * subdirectories (and the root directory). The generated set would be:
 *
 *      0          0     0       0           0
 *    [ ^hello.txt ^ha.c ^subdir ^subdir/... ^/... ]
 *
 * Where the caret ^ indicates the match candidature of the string, the
 * trailing ... means a 'defer point', and the digit 0 indicates
 * the caret state.
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
 * The next step, roughly corresponding to the the wildcard '*', just moves
 * the caret through '?' transitions (which here denotes accept any character)
 *        1          1
 *    [ he^llo.txt ha^.c ]
 *
 *         1          2
 *    [ hel^lo.txt ha.^c ]
 *
 *          1          1
 *    [ hell^o.txt ha.c^ ]
 *
 * On the next step, the ha.c pattern will be at the end of its string, so
 * it is checked to see in an accepting state. Only 5 is an accepting state.
 * So, ha.c is rejected and removed from the list.
 * (Had the string ha.c been marked "deferred", its element would be replaced
 * with those strings from another call to the generator. In this way, ha.c
 * could have been a directory pathname.) 
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
 * removed from the list, and returned as a result.
 */

#include "str.h"

/*
 * Patterns is a set of glob expressions and their reference values.
 * Internally is a state machine capable of matching all of the
 * glob patterns added to it.
 */
struct globset;

/* 
 * Matcher coordinates generators and globs, allowing incremental
 * searching of a generated string space against a globset.
 */
struct matcher;

/** A match is a partially-matched candidate string */
struct match {
	struct match *next;
	const str *str;		/* candidate string (owned) */
	stri stri;		/* position of next byte to match */
	unsigned state;		/* current match state */
	unsigned flags;
#define MATCH_DEFERRED	1 	/* Generator may append more to string */
};

/**
 * Generator is the callback interface that matcher uses to obtain
 * candidate strings to match.
 */
struct generator {
	/**
	 * Main callback function that provides a list of candidate
	 * strings.
	 *
	 * The function should allocate #match structures with #malloc(),
	 * chain them via their #match.next fields, and insert them
	 * into the list indicated by the @a mp parameter.
	 * 
	 * Each #match structure allocated by this function should have its
	 * #match.str field initialized to a string which is
	 *   1. longer then @a prefix
	 *   2. has @a prefix as an initial substring
	 * The #match.flags field may be set to MATCH_DEFERRED.
	 * The #match.pos field may be left uninitialized.
	 * Any other fields should be initialized to zero.
	 *
	 * The MATCH_DEFERRED bit in #match.flags indicates that this
	 * function should be called again to provided more strings.
	 * Such a call will have #match.str passed as the @a prefix
	 * parameter.
	 *
	 * @param mp      address of a #match.next field into which to
	 *                store the new #match elements
	 * @param prefix  The string whose extension is being deferred,
	 *                or the empty string (@c NULL).
	 * @param generator_context The generator context argument given to
	 *                #matcher_new().
	 */ 
	void (*generate)(struct match **mp, const str *prefix, 
				    void *generator_context);
	/**
	 * Releases the context pointer passed to #matcher_new().
	 * This is called by #matcher_free().
	 * This function pointer may be left as @c NULL.
	 * @param generator_context The generator context argument given to
	 *                #matcher_new().
	 */
	void (*free)(void *generator_context);
};

/**
 * Creates a new, empty pattern set.
 * @return a pattern pointer, or NULL on error.
 */
struct globset *globset_new(void);

void globset_free(struct globset *patterns);

/**
 * Adds a glob expression to a pattern set.
 * @param patterns    the set of patterns to add to
 * @param glob        a glob expression
 * @param patternref  the pointer that will be available
 *                    when a matcher matches a string 
 *                    against @a glob
 * @return NULL on success, otherwise an error message because
 *         the glob was probably invalid.
 */
const char *globset_add(struct globset *patterns,
	const str *glob, const void *patternref);

/**
 * Compile the globset into an efficient state.
 * After this, no more globs can be added.
 */
void globset_compile(struct globset *patterns);

/**
 * Allocates a new matcher, which can be used to filter a generated
 * string space.
 * @param patterns  The globset to match against.
 * @param generator Interface for obtaining new candidate strings
 * @param generator_context Context pointer for the generator
 */
struct matcher *matcher_new(const struct globset *patterns,
	const struct generator *generator, void *generator_context);

/** 
 * Searches the generated string space to find the string that matches
 * a pattern from the globset.
 * @param patternref_return  Optional pointer to storage where to
 *                           store the glob's associated pattern reference.
 * @returns the str that matched (caller must free this string),
 *          or @c NULL when the generator is exhausted.
 */
str *matcher_next(struct matcher *matcher, const void **patternref_return);

void matcher_free(struct matcher *matcher);

#endif /* match_h */
