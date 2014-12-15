#ifndef globs_h
#define globs_h

/*
 * A 'globs' is a glob set: a set of string matching expressions each
 * with their own reference values.  It functions as a state machine
 * (automaton) capable of matching strings against any or all of the member
 * glob patterns within it.  The automaton states within the globs are
 * identified by an unsigned integer, and the interfaces below encourage
 * callers to provide their own state storage, treating a globs, once
 * prepared, as an immutable object.
 */

struct globs;

/*
 * Glob Pattern Syntax
 *
 * Glob expressions are best known by their use in the standard
 * Unix shell.
 * This one implements a subset of POSIX extended globs. A glob
 * expression is a sequence of zero or more of the following:
 *
 *      Sub-Pattern...          Matches...
 *      \x                      - the literal character x
 *      ?                       - any character except /
 *      *                       - zero or more ?s
 *      [xy-z]                  - any char in the range set
 *      [!xy-z]                 - any char not in the range set
 *      [^xy-z]                 - any char not in the range set
 *      @(pattern|...)          - exactly 1 of the patterns
 *      ?(pattern|...)          - 0 or 1 of the patterns
 *      *(pattern|...)          - 0 or more "
 *      +(pattern|...)          - 1 or more "
 *      !(pattern|...)          - (NOT SUPPORTED!)
 *      otherwise               - a literal character
 */

/** Creates a new, empty glob set.  */
struct globs *globs_new(void);

/** Deallocate a glob set */
void globs_free(struct globs *globs);

struct str; /* forward decl */

/**
 * Adds a glob expression to the glob set, along with
 * a reference value to return when it matches a string.
 *
 * @param globs       The set of patterns to add to
 * @param globstr     A UTF-8 string containing an extended
 *                    glob expression. (NOT TAKEN) (See above)
 * @param ref         The pointer that will be returned
 *                    by #globs_is_accept()
 * @return @c NULL on success,
 *         otherwise an error message
 */
const char *globs_add(struct globs *globs,
	const struct str *globstr, const void *ref);

/**
 * Compile the globs into an efficient state.
 * After this, no more globs can be added.
 */
void globs_compile(struct globs *globs);

/**
 * Tries to advance a globs match state.
 * @param globs   the set of globs
 * @param ch     the character to advance
 * @param statep pointer to the state to advance
 * @returns non-zero if the state advanced, or
 *          0 if the character was rejected.
 */
int globs_step(const struct globs *globs, unsigned ch, unsigned *statep);

/**
 * Tests if the given state is an accept state.
 * @param globs   the set of globs
 * @param state  the state being tested
 * @returns @c NULL if the state is not an accept state,
 *          or the @a ref argument supplied to #globs_add()
 */
const void *globs_is_accept_state(const struct globs *globs, unsigned state);

#endif /* globs_h */
