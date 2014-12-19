#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "str.h"
#include "prereq.h"

static void prereq_tostr_(const struct prereq *p, char **s);

static void
prereq_any_tostr_(const struct prereq *p, char **s)
{
	if (!p) {
		*(*s)++ = '0';
		return;
	}
	if (p->type == PR_FALSE)
		return;
	if (p->type != PR_ANY) {
		*(*s)++ = '?';
		prereq_tostr_(p, s);
		return;
	}
	prereq_any_tostr_(p->any.left, s);			/* left */
	if (p->any.left && p->any.left->type != PR_FALSE)
		*(*s)++ = ' ';
	prereq_tostr_(p->any.right, s);				/* right */
}

static void
prereq_all_tostr_(const struct prereq *p, char **s)
{
	while (p && p->type == PR_ALL) {
		prereq_tostr_(p->all.left, s);			/* left */
		if (!p->all.right || p->all.right->type != PR_TRUE)
			*(*s)++ = ' ';
		p = p->all.right;				/* right */
	}
	if (!p)
		*(*s)++ = '0';
	else if (p->type != PR_TRUE) {
		*(*s)++ = '?';
		prereq_tostr_(p->all.left, s);
	}
}

static void
prereq_tostr_(const struct prereq *p, char **s)
{
	stri i;

	if (!p) {
		*(*s)++ = '0';
		return;
	}

	switch (p->type) {
	case PR_STATE:
		for (i = stri_str(p->state); stri_more(i); stri_inc(i)) {
			*(*s)++ = stri_at(i);
		}
		break;
	case PR_NOT:
		*(*s)++ = '!';
		prereq_tostr_(p->not, s);
		break;
	case PR_ALL:
	case PR_TRUE:
		*(*s)++ = '(';
		prereq_all_tostr_(p, s);
		*(*s)++ = ')';
		break;
	case PR_ANY:
	case PR_FALSE:
		*(*s)++ = '{';
		prereq_any_tostr_(p, s);
		*(*s)++ = '}';
		break;
	default:
		*(*s)++ = '?';
		break;
	}
}

#define assert_prereq_eq(p, expected) \
	assert_prereq_eq_(__FILE__, __LINE__, p, expected)

static void
assert_prereq_eq_(const char *file, unsigned lineno, const struct prereq *p, const char *expected)
{
	char actual[2048];
	char *s = actual;

	if (p && (p->type == PR_ALL || p->type == PR_TRUE))
		prereq_all_tostr_(p, &s);
	else
		prereq_tostr_(p, &s);
	*s = '\0';

	if (strcmp(expected, actual) != 0) {
		fprintf(stderr, "%s:%d: mismatch\n"
				"  expected: '%s'\n  actual: '%s'\n",
			file, lineno, expected, actual);
		abort();
	}
}

/*
 * Checks that the prerequisite expression will be parsed
 * into a structure, and then unparsed back into a standard form
 * that should be the same.
 */
#define check_prereq(source) check_prereq_(__FILE__, __LINE__, source)
static void
check_prereq_(const char *file, unsigned lineno, const char *source)
{
	STR source_str = str_new(source);
	const char *error = 0;
	struct prereq *p = prereq_make(source_str, &error);
	if (!p || error) {
		fprintf(stderr, "%s:%d: error parsing '%s'"
				"\n  error: '%s'\n",
				file, lineno, source, error);
		abort();
	}
	assert_prereq_eq_(file, lineno, p, source);
	prereq_free(p);
}

#define assert_parse_fail(source) assert_parse_fail_(__FILE__, __LINE__, source)
static void
assert_parse_fail_(const char *file, unsigned lineno, const char *source)
{
	STR source_str = str_new(source);
	const char *error = 0;
	struct prereq *p;

	if ((p = prereq_make(source_str, &error)) != 0 || error == 0) {
		char result[2048], *s = result;
		prereq_tostr_(p, &s);
		*s = '\0';
		fprintf(stderr, "%s:%d: expected failure but got success\n"
				"  source: '%s'\n"
				"  result: '%s'\n",
				file, lineno, source, result);
		abort();
	}
}

int
main()
{
	check_prereq("a@1");
	check_prereq("{a b c}");
	check_prereq("{a}");
	check_prereq("(h)");
	check_prereq("{}");
	check_prereq("()");
	check_prereq("");
	check_prereq("a b c");
	check_prereq("!!a");
	check_prereq("(a b) c");
	check_prereq("(a {x y (i)} x) c");
	assert_parse_fail(")");
	assert_parse_fail("{");
	assert_parse_fail("a (");
	assert_parse_fail("(x}");

	return 0;
}
