#ifndef globs_h
#define globs_h

struct str;

/*
 * A set of glob expressions and their reference values.
 * Internally this is a state machine (automaton) capable of matching
 * strings against any or all of the glob patterns within to it.
 * The states within the glob set are identified by unsigned integers.
 */
struct globs;

/** Creates a new, empty set of globs.  */
struct globs *globs_new(void);

/** Deallocate a set of globs */
void globs_free(struct globs *globs);

/**
 * Adds a glob expression to the glob set, along with
 * a reference value to return when it matches a string.
 *
 * @param globs       The set of patterns to add to
 * @param globstr     A string containing an extended
 *                    glob expression.
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
