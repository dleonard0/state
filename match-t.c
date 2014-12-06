#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "match.h"
#include "globs.h"
#include "str.h"

/*------------------------------------------------------------
 * constructors for trees and globs
 */

/* auto-cleanup globs: GLOBS g = make_globs(...); */
#define GLOBS struct globs * __attribute((__cleanup__(globs_cleanup)))
static void globs_cleanup(struct globs **g) { globs_free(*g); }
#define make_globs(...) make_globs_(__FILE__, __LINE__, __VA_ARGS__, NULL)
static struct globs *make_globs_(const char *file, int lineno, ...) 
	__attribute__((sentinel));

/* A tree simulates a filesystem tree  */
struct tree;
#define TREE struct tree * __attribute((__cleanup__(tree_cleanup)))
static void tree_cleanup(struct tree **node);
#define make_tree(...) make_tree_(__FILE__, __LINE__, __VA_ARGS__, NULL)
static struct tree *make_tree_(const char *file,int lineno,...)
	__attribute__((sentinel));

/* asserts that the globs applied to the tree match only the names given */
#define assert_matches(globs, tree, ...) do { 				\
	const char *_expected[] = { __VA_ARGS__, 0 };			\
	assert_matches_(__FILE__, __LINE__, globs, tree, _expected);	\
    } while (0)
static void assert_matches_(const char *file, int lineno,
	struct globs *globs, struct tree *tree, const char **expected);

static struct globs *
make_globs_(const char *file, int lineno, ...)
{
	va_list ap;
	const char *s;
	struct globs *globs = globs_new();

	va_start(ap, lineno);
	while ((s = va_arg(ap, const char *))) {
		str *ss = str_new(s);
		globs_add(globs, ss, s);
		str_free(ss);
	}
	globs_compile(globs);
	return globs;
}

struct tree {
	const char *path;
	unsigned pathlen;
	struct tree *sibling, *child;
};

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
				    strncmp((*node)->path, c, clen))
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

struct test_context {
	int freed;
	struct tree *tree;
};

struct match **
test_generate(struct match **mp, const str *prefix, void *gcontext)
{
	struct test_context *ctxt = gcontext;

	/*
	 * Search the tree for the prefix, then return a list of the
	 * found node's children.
	 */
	struct tree *node = ctxt->tree;
	char buf[1024], *b;
	stri i;
	for (b = buf, i = stri_str(prefix); stri_more(i); stri_inc(i)) {
		*b++ = stri_at(i) & 0x7f;
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
	    assert(node); // abort if called with a deferred we never gen'd
	    node = node->child;
	    p += plen;
	}
	for (; node; node = node->sibling) {
		str *mstr, **x;
		x = str_xcats(str_xcat(&mstr, prefix), node->path);
		if (node->child) {
			x = str_xcats(x, "/");
		}
		*x = 0;
		struct match *newm = match_new(mstr);
		if (node->child)
			newm->flags |= MATCH_DEFERRED;
		*mp = newm;
		mp = &newm->next;
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

static void
assert_matches_(const char *file, int lineno,
	struct globs *globs, struct tree *tree, const char **expected)
{
	const char **e;
	struct test_context tctxt;

	for (e = expected; *e; e++) ;
	unsigned nexpected = e - expected;
	char *expected_seen = calloc(nexpected + 1, 1);

	tctxt.freed = 0;
	tctxt.tree = tree;

	struct matcher *matcher = matcher_new(globs, &test_generator, &tctxt);
	unsigned matchremain = nexpected;
	unsigned i;
	int error = 0;

	while (!error) {
		const void *ref;
		str *result = matcher_next(matcher, &ref);
		if (matchremain == 0) {
			if (result) {
				fprintf(stderr, "%s:%d: "
					"expected %u more matches",
					file, lineno, matchremain);
				error = 1;
			}
			break;
		}
		for (i = 0; i < nexpected; ++i) {
			if (str_eq(result, expected[i])) {
				if (expected_seen[i]) {
					fprintf(stderr, "%s:%d: "
						"duplicate match '%s'\n",
						file, lineno,
						expected[i]);
					error = 1;
					break;
				}
				expected_seen[i] = 1;
				matchremain--;
				break;
			}
		}
		if (i == nexpected) { /* for loop completed without match */
			fprintf(stderr, "%s:%d: unexpected match '",
				file, lineno);
			stri i;
			for (i = stri_str(result); stri_more(i); stri_inc(i))
				putc(stri_at(i) & 0x7f, stderr);
			fprintf(stderr, "'\n");
			error = 1;
		}
	}
	if (error) {
		exit(1);
	}
	free(expected_seen);
	matcher_free(matcher);
}

int
main()
{

	{
		GLOBS g = make_globs("a");
		TREE t = make_tree("a", "b", "c");
		assert_matches(g, t, "a");

	}
	// TODO more tests

	return 0;
}
