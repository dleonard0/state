#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "match.h"
#include "globs.h"
#include "str.h"
#include "nfa-dbg.h"

static int Debug;

/*------------------------------------------------------------
 * convenience constructors for trees and globs
 */

/**
 * A type for automatically released globs. Usage:
 *     GLOBS g = make_globs(...);
 */
#define GLOBS struct globs * __attribute((__cleanup__(globs_cleanup)))

#define make_globs(...) make_globs_(__FILE__, __LINE__, __VA_ARGS__, NULL)
static struct globs *make_globs_(const char *file, int lineno, ...)
	__attribute__((sentinel));
static struct globs *
make_globs_(const char *file, int lineno, ...)
{
	va_list ap;
	const char *s;
	struct globs *globs = globs_new();

	va_start(ap, lineno);
	while ((s = va_arg(ap, const char *))) {
		int len = strlen(s);
		const void *ref = s;
		/* If the glob ends with "=N" then make "N" the ref */
		if (len > 2 && s[len-2] == '=') {
			ref = &s[len-1];
			len -= 2;
		}
		str *ss = str_newn(s, len);
		globs_add(globs, ss, ref);
		str_free(ss);
	}
	globs_compile(globs);
	if (Debug) {
		fprintf(stderr, "%s:%d: globs:\n", file, lineno);
		nfa_dump(stderr, (const struct nfa *)globs, 0);
	}
	return globs;
}

static void globs_cleanup(struct globs **g) { globs_free(*g); }

/*------------------------------------------------------------
 * In-memory file tree
 */

/**
 * A tree simulates a filesystem tree.
 * It's an in-memory structure of 'directories' and 'files' for names only.
 */
struct tree {
	const char *path;
	unsigned pathlen;
	struct tree *sibling, *child;
};

#define TREE struct tree * __attribute((__cleanup__(tree_cleanup)))

/**
 * Creates an in-memory filesystem tree from a list of /-separated
 * pathnames. Interior directory nodes are automatically created.
 *
 * @param ... C strings indicating full paths, such as "a/b/c"
 *
 * @return the root of the tree structure.
 */
#define make_tree(...) make_tree_(__FILE__, __LINE__, __VA_ARGS__, NULL)
static struct tree *make_tree_(const char *file,int lineno,...)
	__attribute__((sentinel));
static struct tree *
make_tree_(const char *file, int lineno, ...)
{
	struct tree *root = 0; /* first child of virtual root */
	va_list ap;
	const char *s;
	va_start(ap, lineno);
	while ((s = va_arg(ap, const char *))) {
		unsigned clen = 0;
		const char *c = s;
		struct tree **node = &root;

		for (;;) {
			while (*c == '/') c++;
			/* pick out the first path component as c[:clen] */
			for (clen = 0; c[clen]; clen++) {
				if (c[clen] == '/')
					break;
			}
			if (!c[clen] || !c[clen + 1]) /* allow trailing slash */
				break;
			/* move node to the sibling with c's name */
			while (*node) {
				if ((*node)->pathlen == clen &&
				    strncmp((*node)->path, c, clen) == 0)
					break;
				node = &(*node)->sibling;
			}
			if (!*node) {
				fprintf(stderr, "%s:%d: missing explicit parent"
					" '%.*s' in '%s'\n",
					file, lineno, clen, c, s);
				exit(1);
			}
			c += clen;
			/* Move down into the selected node's child list */
			node = &(*node)->child;
		}
		if (!clen) {
			fprintf(stderr, "%s:%d: empty component in '%s'\n",
				file, lineno, s);
			exit(1);
		}

		/* find first sibling with lexicographically larger name */
		while (*node) {
			size_t minlen = (*node)->pathlen < clen
				      ? (*node)->pathlen
				      : clen;
			int cmp = strncmp(c, (*node)->path, minlen);
			if (cmp < 0)
				break;
			if (cmp == 0 && clen > (*node)->pathlen)
				break;
			if (cmp == 0 && clen == (*node)->pathlen) {
				fprintf(stderr, "%s:%d: duplicate '%s'\n",
					file, lineno, s);
				exit(1);
			}
			node = &(*node)->sibling;
		}

		struct tree *cnode = malloc(sizeof *cnode);
		cnode->path = c;
		cnode->pathlen = clen;
		cnode->child = 0;

		/* insert */
		cnode->sibling = *node;
		*node = cnode;
	}
	return root;
}

/** Releases a tree structure */
static void
tree_cleanup(struct tree **node)
{
	while (*node) {
		struct tree *sibling = (*node)->sibling;
		tree_cleanup(&(*node)->child);
		free(*node);
		*node = sibling;
	}
}

/** 
 * Prints a tree structure, for human debug purposes.
 *
 * @param f       where to print the tree to
 * @param node    the subtree to print
 * @param prefix  the string to prefix each node name with
 */
static void
dump_tree(FILE *f, const struct tree *node, char *prefix)
{
	for (; node; node = node->sibling) {
	    fprintf(f, "  %s%.*s\n", prefix, node->pathlen, node->path);
	    if (node->child) {
		char buf[1024];
		snprintf(buf, sizeof buf, "%s%.*s/", prefix,
			node->pathlen, node->path);
		dump_tree(f, node->child, buf);
	    }
	}
}

/*------------------------------------------------------------
 * Implementation of the match generator callback interface.
 */

struct test_context {
	int freed;
	struct tree *tree;
};

struct match **
test_generate(struct match **mp, const str *prefix, void *gcontext)
{
	struct test_context *ctxt = gcontext;
	stri i;

	if (Debug) {
		fprintf(stderr, "  ");
		for (i = stri_str(prefix); stri_more(i); stri_inc(i))
			putc(stri_at(i), stderr);
		if (!prefix) fprintf(stderr, "\"\"");
		fprintf(stderr, ":\n");
	}
	/*
	 * Search the tree for the prefix, then return a list of the
	 * found node's children.
	 */
	struct tree *node = ctxt->tree;
	char buf[1024], *b;
	for (b = buf, i = stri_str(prefix); stri_more(i); stri_inc(i)) {
		*b++ = stri_at(i);
	}
	*b = '\0';
	assert(!buf[0] || b[-1] == '/');
	char *p = buf;
	while (*p) {
	    unsigned plen;

	    while (*p == '/')
		p++;
	    if (!*p)
		break;
	    for (plen = 0; p[plen]; plen++)
		if (p[plen] == '/')
			break;
	    for (; node; node = node->sibling)
		if (node->pathlen == plen && strncmp(node->path, p, plen) == 0)
			break;
	    assert(node); /* abort if called with a deferred we never gen'd */
	    node = node->child;
	    p += plen;
	}
	for (; node; node = node->sibling) {
		str *mstr, **x;
		x = str_xcatsn(str_xcat(&mstr, prefix),
				node->path, node->pathlen);
		if (node->child) {
			x = str_xcats(x, "/");
		}
		*x = 0;
		struct match *newm = match_new(mstr);
		if (node->child)
			newm->flags |= MATCH_DEFERRED;
		*mp = newm;
		mp = &newm->next;
		if (Debug) {
			fprintf(stderr, "    ");
			for (i = stri_str(newm->str); stri_more(i); stri_inc(i))
				putc(stri_at(i), stderr);
			if (newm->flags & MATCH_DEFERRED)
				fprintf(stderr, " ...");
			fprintf(stderr, "\n");
		}
	};
	return mp;
}

static void
test_free(void *gcontext)
{
	struct test_context *ctxt = gcontext;
	assert(!ctxt->freed);
	ctxt->freed = 1;
}

static struct generator test_generator = {
	.generate = test_generate,
	.free = test_free,
};

/**
 * Asserts that the globs applied to the tree match only the names given
 * Aborts on failure.
 *
 * @param globs   the glob set
 * @param tree    the tree to match globs against
 * @param ...     The C strings that are only expected to be in the result set.
 */
#define assert_matches(globs, tree, ...) do {				\
	const char *_expected[] = { __VA_ARGS__, 0 };			\
	assert_matches_(__FILE__, __LINE__, globs, tree, _expected);	\
    } while (0)
static void
assert_matches_(const char *file, int lineno,
	struct globs *globs, struct tree *tree, const char **expected)
{
	const char **e;
	struct test_context tctxt;
	unsigned i;

	if (Debug) {
		fprintf(stderr, "%s:%d: tree:\n", file, lineno);
		dump_tree(stderr, tree, "");
	}

	/* count the number of expected strings */
	e = expected;
	while (*e) e++;
	unsigned nexpected = e - expected;

	/* construct an array of expected info */
	struct {
		const char *exp;	/* string to expect */
		char seen;		/* have we seen it already */
		unsigned len;		/* length of exp */
		const char *ref;	/* reference to expect, or 0 */
	} *exp = malloc(nexpected * sizeof *exp);

	for (i = 0; i < nexpected; i++) {
		/* if the expected string ends with "=N" then
		 * use "N" as the expected reference to strcmp */
		unsigned len = strlen(expected[i]);
		exp[i].exp = expected[i];
		exp[i].seen = 0;
		if (len > 2 && expected[i][len - 2] == '=') {
			exp[i].len = len - 2;
			exp[i].ref = expected[i] + len - 1;
		} else {
			exp[i].len = len;
			exp[i].ref = 0;
		}
	}

	tctxt.freed = 0;
	tctxt.tree = tree;

	if (Debug)
		fprintf(stderr, "%s:%d: matching...\n", file, lineno);

	struct matcher *matcher = matcher_new(globs, &test_generator, &tctxt);
	unsigned matchremain = nexpected;

	int error = 0;
	while (!error) {
		const void *ref = 0;
		str *result = matcher_next(matcher, &ref);
		if (Debug) {
			fprintf(stderr, "  * ");
			stri si;
			for (si = stri_str(result); stri_more(si); stri_inc(si))
				putc(stri_at(si), stderr);
			if (!result) fprintf(stderr, "(null)");
			if (result && ref)
				fprintf(stderr, " = %s", (const char *)ref);
			fprintf(stderr, "\n");
		}
		if (matchremain == 0) {
			if (result) {
				fprintf(stderr, "%s:%d: "
					"expected %u more matches",
					file, lineno, matchremain);
				error = 1;
			}
			str_free(result);
			break;
		}
		for (i = 0; i < nexpected; ++i) {
			if (str_eqn(result, exp[i].exp, exp[i].len)) {
				if (exp[i].seen) {
					fprintf(stderr, "%s:%d: "
						"duplicate match '%s'\n",
						file, lineno,
						exp[i].exp);
					error = 1;
					break;
				}
				if (exp[i].ref &&
				    strcmp(exp[i].ref, (const char *)ref) != 0)
				{
					fprintf(stderr, "%s:%d: "
						"unexpected '%.*s=%s' "
						"while matching '%s'\n",
						file, lineno,
						exp[i].len, exp[i].exp,
						(const char *)ref,
						exp[i].exp);
					error = 1;
				}
				exp[i].seen = 1;
				matchremain--;
				break;
			}
		}
		if (i == nexpected) { /* for loop completed without match */
			fprintf(stderr, "%s:%d: unexpected match '",
				file, lineno);
			stri i;
			for (i = stri_str(result); stri_more(i); stri_inc(i))
				putc(stri_at(i), stderr);
			fprintf(stderr, "'\n");
			error = 1;
		}
		str_free(result);
	}
	if (error) {
		exit(1);
	}
	free(exp);
	matcher_free(matcher);
}

/** Tests an environment variable e is set to indicate true */
static int
testenv(const char *e)
{
	char *c = getenv(e);
	return c && *c && *c != '0' && *c != 'n';
}

int
main()
{
	Debug = testenv("DEBUG");

	{
		GLOBS g = make_globs("a=1");
		TREE t = make_tree("a", "b");
		assert_matches(g, t, "a=1");

	}
	{
		GLOBS g = make_globs("*/*");
		TREE t = make_tree("a/", "a/b", "a/c", "b");
		assert_matches(g, t, "a/b", "a/c");
	}

	return 0;
}
