#include <assert.h>
#include "match.h"
#include "str.h"
#include "fsgen.h"

/* Unit tests for the filesystem generator implementation */

/**
 * Searches a list of matches for the string s.
 *
 * @returns @c NULL if s is not found in the match list
 */
static const struct match *
mfind(const struct match *matches, const char *s)
{
	const struct match *m;

	for (m = matches; m; m = m->next) {
		if (str_eq(m->str, s))
			break;
	}
	return m;
}

/**
 * Searches a list of matches for the deferred string s.
 *
 * @returns @c NULL if s is not found in the match list, or
 *                  if s was found but it wasn't marked deferred.
 */
static const struct match *
mfind_def(const struct match *matches, const char *s)
{
	const struct match *m = mfind(matches, s);
	if (m && !(m->flags & MATCH_DEFERRED))
		m = 0;
	return m;
}

/**
 * Searches a list of matches for the non-deferred string s.
 *
 * @returns @c NULL if s is not found in the match list, or
 *                  if s was found but it WAS marked deferred.
 */
static const struct match *
mfind_undef(const struct match *matches, const char *s)
{
	const struct match *m = mfind(matches, s);
	if (m && (m->flags & MATCH_DEFERRED))
		m = 0;
	return m;
}

/** Frees a list of matches */
static void
matches_free(struct match **mp)
{
	while (*mp) {
		struct match *m = *mp;
		*mp = m->next;
		match_free(m);
	}
}

int
main()
{
	{
		struct match *matches, **mp;

		/* Enumerate the current directory */
		mp = fs_generate(&matches, 0);
		*mp = 0;

		assert(mfind_def(matches, "/"));
		assert(mfind_def(matches, "./"));
		assert(mfind_undef(matches, "."));

		/*
		 * NOTE: These tests assumes the filesystem actually
		 * contains the file /bin/rm
		 */
		struct match *root_matches;
		const struct match *root;
		root = mfind_def(matches, "/");
		mp = fs_generate(&root_matches, root->str);
		*mp = 0;
		assert(mfind_def(root_matches, "/bin/"));
		assert(mfind_undef(root_matches, "/bin"));

		struct match *bin_matches;
		const struct match *bin;
		bin = mfind_def(root_matches, "/bin/");
		mp = fs_generate(&bin_matches, bin->str);
		*mp = 0;
		assert(mfind_undef(bin_matches, "/bin/rm"));

		matches_free(&bin_matches);
		matches_free(&root_matches);
		matches_free(&matches);
	}
	return 0;
}
