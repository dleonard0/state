#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "str.h"
#include "prereq.h"

static void
prereq_tostr_(const struct prereq *p, char **s)
{
	stri i;
	int first = 1;

	for (; p; p = p->next) {
		if (first) first = 0;
		else       *(*s)++ = ' ';
		switch (p->type) {
		case PR_STATE:
			for (i = stri_str(p->state); stri_more(i); stri_inc(i)) {
				*(*s)++ = stri_at(i);
			}
			break;
		case PR_ANY:
			*(*s)++ = '{';
			prereq_tostr_(p->any, s);
			*(*s)++ = '}';
			break;
		case PR_ALL:
			*(*s)++ = '(';
			prereq_tostr_(p->any, s);
			*(*s)++ = ')';
			break;
		case PR_NOT:
			*(*s)++ = '!';
			prereq_tostr_(p->not, s);
			break;
		default:
			*(*s)++ = '?';
			break;
		}
	}
}

#define assert_prereq_eq(p, expected) \
	assert_prereq_eq_(__FILE__, __LINE__, p, expected)

static void
assert_prereq_eq_(const char *file, unsigned lineno, const struct prereq *p, const char *expected)
{
	char actual[2048];
	char *s = actual;
	if (p && p->type == PR_ALL)
		p = p->all;
	prereq_tostr_(p, &s);
	*s = '\0';

	if (strcmp(expected, actual) != 0) {
		fprintf(stderr, "%s:%d: mismatch\n"
				"  expected: '%s'\n  actual: '%s'\n",
			file, lineno, expected, actual);
		abort();
	}
}

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

#define fail_parse(source) fail_parse_(__FILE__, __LINE__, source)
static void
fail_parse_(const char *file, unsigned lineno, const char *source)
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
	fail_parse(")");

	return 0;
}
