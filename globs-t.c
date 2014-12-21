#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>

#include "globs.h"
#include "str.h"
#include "nfa-dbg.h"

/* Unit tests for glob patterns */

/**
 * A keyword string passed to #assert_accepts()/#assert_rejects() to indicate
 * that the acceptance of subsequent terms should be inverted.
 */
static const char * const NOT = "NOT";

/** Prints a representation of a compiled globs to stdout, for debugging */
static void
globs_dump(const struct globs *g, const char *gexp, int state)
{
	printf("glob '%s' compiled to:\n", gexp);
	nfa_dump(stdout, (const struct nfa *)g, state);
	fflush(stdout);
}

/**
 * Asserts that a glob expression compiles and then succeeds (or fails) to
 * match the given test string(s).
 *
 * @param globexp  a C string that will be coverted into a glob DFA
 * @param ...      test C strings that globexp will be matched against.
 *                 The special value #NOT is used to reverse test sense
 *                 of subsequent test strings.
 */
#define assert_accepts(...) assert_match_(__FILE__,__LINE__,1,__VA_ARGS__,NULL)
#define assert_rejects(...) assert_match_(__FILE__,__LINE__,0,__VA_ARGS__,NULL)
static void assert_match_(const char *file, int lineno, int expect_accept,
			  const char *globexp, ...)
	__attribute__((sentinel));

static void
assert_match_(const char *file, int lineno, int expect_accept,
	      const char *globexp, ...)
{
	va_list ap;
	const char *text;

	struct globs *g = globs_new();
	STR str = str_new(globexp);
	globs_add(g, str, globexp);
	globs_compile(g);

	va_start(ap, globexp);
	while ((text = va_arg(ap, const char *)) != NULL) {
		unsigned state = 0;
		const char *t;
		int accepted = 0;

		if (text == NOT) {
			expect_accept = !expect_accept;
			continue;
		}

		for (t = text; *t; ++t) {
			if (!globs_step(g, *t, &state))
				break;
		}
		accepted = !*t && (globs_is_accept_state(g, state) == globexp);
		if (expect_accept && !accepted) {
			globs_dump(g, globexp, state);
			fprintf(stderr, "%s:%d: glob '%s' failed to accept\n"
				        "  string '%s'\n"
					"  at      %*s^\n",
				       file, lineno, globexp, text, t-text, "");
			abort();
		}
		if (!expect_accept && accepted) {
			globs_dump(g, globexp, state);
			fprintf(stderr, "%s:%d: glob '%s' failed to reject\n"
				        "  string '%s'\n"
					"  at      %*s^\n",
				       file, lineno, globexp, text, t-text, "");
			abort();
		}
	}
	globs_free(g);
}

int
main()
{
	/* reference constants for testing */
	const void * const refA = "A";

	/*
	 * Match tests were known to go into infinite loops
	 * so ask for a SIGALRM in 5 seconds
	 */
	/*alarm(5);*/

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
	{
		assert_accepts("", "",
				NOT, "a", "0");
		assert_accepts("[abc]", "a","b","c",
				NOT, "x",""," a", "aa");
		assert_accepts("@(a|b|c)", "a", "b", "c",
				NOT, "", "d", "abc", "a|b|c");
		assert_accepts("@(a)", "a",
				NOT, "", "aa");
		assert_accepts("foo*bar", "foobar", "foo-bar", "foofoobar",
				"foobarbar",
				NOT, "foo", "bar", "fobar", "fbar", "foo/bar");
		assert_accepts("?(@(a|b)|c)", "", "a", "b", "c",
				NOT, "ac", "d");
		assert_accepts("*(*(a))", "", "a", "aa", "aaa",
				NOT, " a");
	}
	return 0;
}
