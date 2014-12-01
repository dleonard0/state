#include <assert.h>
#include "globs.h"
#include "str.h"

struct globs *globs_new(void);
void globs_free(struct globs *globs);
const char *globs_add(struct globs *globs, const struct str *globstr, const void *ref);
void globs_compile(struct globs *globs);
int globs_step(const struct globs *globs, unsigned ch, unsigned *statep);
const void *globs_is_accept_state(const struct globs *globs, unsigned state);

int
main()
{
	/* reference constants for testing */
	const void * const refA = "A";
	//const void * const refB = "B";
	//const void * const refC = "C";

	{
		STR empty_str = str_new("");
		struct globs *g = globs_new();
		globs_add(g, empty_str, refA);
		globs_compile(g);
		assert(globs_is_accept_state(g, 0) == refA);
		unsigned state = 0;
		assert(!globs_step(g, 'x', &state));
		assert(state == 0);
		assert(!globs_step(g, '\0', &state));
		assert(state == 0);
		globs_free(g);
	}
	{
		STR expr = str_new("?b");
		struct globs *g = globs_new();
		unsigned state;
		globs_add(g, expr, refA);
		globs_compile(g);
		state = 0;
		assert(!globs_is_accept_state(g, state));
		assert(globs_step(g, 'a', &state));
		assert(state != 0);
		assert(!globs_is_accept_state(g, state));
		const unsigned oldstate = state;
		assert(!globs_step(g, 'x', &state));
		assert(state == oldstate);
		assert(globs_step(g, 'b', &state));
		assert(state != oldstate);
		assert(globs_is_accept_state(g, state) == refA);
		globs_free(g);
	}
	return 0;
}
