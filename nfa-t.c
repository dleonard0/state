#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nfa.h"
#include "nfa-dbg.h"

/* Debug is set to 1 if environment variable $DEBUG is defined */
static int Debug = 0;

/* Only print when debugging */
#define dprintf(...) do { if (Debug) printf(__VA_ARGS__); } while (0)

/* A subnfa structure used during construction
 * of test regular expressions. The subnfa represents
 * a regular expression that is part of a larger regex. */
struct subnfa {
	unsigned entry;
	unsigned exit;
};

/*
 * For testing, this is the regular expression language
 * that the test patterns use. A recursive descent parser
 * naively turns this into an NFA.
 *
 * E ::= F
 * F ::= S
 *   |   F '|' S
 * S ::= empty
 *   |   S T
 * T ::= A '?' | A '*' | A
 * A ::= '[' ( char | char '-' char )* ']'
 *   |   '.'
 *   |   char
 *   |   '(' E ')'
 */

static struct subnfa parse_exp(struct nfa *nfa, const char **sp);

/* A ::= [c-c] | . | c | (E) */
static struct subnfa
parse_atom(struct nfa *nfa, const char **sp)
{
	struct subnfa ret;
	cclass *cc = NULL;
	char ch;
	struct edge *edge;

	ch = *(*sp)++;
	if (ch == '(') {
		ret = parse_exp(nfa, sp);
		ch = *(*sp)++;
		assert(ch == ')');
		return ret;
	}

	cc = cclass_new();
	if (ch == '[') {
		while (**sp && **sp != ']') {
		    unsigned lo, hi;
		    if (**sp == '\\') (*sp)++;
		    lo = *(*sp)++;
		    if (**sp == '-') {
			++*sp;
			if (**sp == ']')
			    hi = MAXCHAR;
			else {
			    if (**sp == '\\') ++*sp;
			    hi = (*(*sp)++) + 1;
			}
		    } else hi = lo + 1;
		    cclass_add(cc, lo, hi);
		}
		ch = *(*sp)++;
		assert(ch == ']');
	} else if (ch == '.') {
		cclass_add(cc, 0, MAXCHAR);
	} else if (ch == '\\') {
		ch = *(*sp)++;
		cclass_add(cc, ch, ch + 1);
	} else {
		cclass_add(cc, ch, ch + 1);
	}
	ret.entry = nfa_new_node(nfa);
	ret.exit = nfa_new_node(nfa);
	edge = nfa_new_edge(nfa, ret.entry, ret.exit);
	edge->cclass = cc;
	return ret;
}

/* T ::= A? | A* | A */
static struct subnfa
parse_term(struct nfa *nfa, const char **sp)
{
	char ch;
	struct subnfa ret = parse_atom(nfa, sp);

	while ((ch = **sp) == '*' || ch == '?') {
		struct subnfa sub = ret;
		ret.entry = nfa_new_node(nfa);
		ret.exit = nfa_new_node(nfa);
		nfa_new_edge(nfa, ret.entry, sub.entry);
		nfa_new_edge(nfa, sub.exit, ret.exit);
		nfa_new_edge(nfa, sub.entry, sub.exit);
		if (ch == '*')
			nfa_new_edge(nfa, sub.exit, sub.entry);
		++*sp;
	}
	return ret;
}

/* S ::= empty | S T */
static struct subnfa
parse_sequence(struct nfa *nfa, const char **sp)
{
	struct subnfa ret;
	unsigned mid;

	ret.entry = nfa_new_node(nfa);
	mid = nfa_new_node(nfa);
	nfa_new_edge(nfa, ret.entry, mid);
	while (**sp && **sp != '|' && **sp != ')') {
		struct subnfa next = parse_term(nfa, sp);
		nfa_new_edge(nfa, mid, next.entry);
		mid = nfa_new_node(nfa);
		nfa_new_edge(nfa, next.exit, mid);
	}
	ret.exit = mid;
	return ret;
}

/* F ::= S  |  F '|' S */
static struct subnfa
parse_factor(struct nfa *nfa, const char **sp)
{
	struct subnfa ret = parse_sequence(nfa, sp);

	while (**sp == '|') {
		struct subnfa alt = ret;
		ret.entry = nfa_new_node(nfa);
		ret.exit = nfa_new_node(nfa);
		nfa_new_edge(nfa, ret.entry, alt.entry);
		nfa_new_edge(nfa, alt.exit, ret.exit);
		++*sp; /* '|' */
		alt = parse_sequence(nfa, sp);
		nfa_new_edge(nfa, ret.entry, alt.entry);
		nfa_new_edge(nfa, alt.exit, ret.exit);
	}
	return ret;
}

/* E ::= F */
static struct subnfa
parse_exp(struct nfa *nfa, const char **sp)
{
	return parse_factor(nfa, sp);
}

/* Creates an NFA graph from a test regular expression string */
static struct nfa *
make_nfa(const char *s)
{
	struct subnfa res, sub;
	struct nfa *nfa = nfa_new();

	res.entry = nfa_new_node(nfa);
	res.exit = nfa_new_node(nfa);
	nfa_add_final(nfa, res.exit, s);

	sub = parse_exp(nfa, &s);
	nfa_new_edge(nfa, res.entry, sub.entry);
	nfa_new_edge(nfa, sub.exit, res.exit);

	return nfa;
}

/** Checks a graph to see if it really is a DFA. Aborts on error */
static void
assert_deterministic(const struct nfa *dfa)
{
	unsigned i;
	unsigned nfinals = 0;

	assert(dfa->nnodes > 0);
	for (i = 0; i < dfa->nnodes; ++i) {
		const struct node *n = &dfa->nodes[i];
		unsigned j;
		cclass *allcc = cclass_new();
		for (j = 0; j < n->nedges; ++j) {
			const struct edge *edge = &n->edges[j];
			assert(edge->cclass); /* no epsilons */
			assert(edge->dest < dfa->nnodes);
			/* determinism check: */
			assert(!cclass_intersects(edge->cclass, allcc));
			cclass_addcc(allcc, edge->cclass);
		}
		if (cclass_is_empty(allcc)) {
			assert(n->nfinals); /* only finals can dead-end */
		}
		cclass_free(allcc);
		assert(n->nfinals < 2);
		nfinals += n->nfinals;
	}
	assert(nfinals > 0);
}

/* Tests if a string is accepted by a DFA */
static int
dfa_matches(const struct nfa *dfa, const char *str)
{
	unsigned state = 0;
	const char *s;

	assert_deterministic(dfa);

	dprintf("dfa_matches: \"%s\":", str);
	for (s = str; *s; ++s) {
		char ch = *s;
		const struct node *n = &dfa->nodes[state];
		const struct edge *edge = 0;
		unsigned j;

		dprintf(" %u '%c'", state, ch);

		for (j = 0; j < n->nedges; ++j) {
			if (cclass_contains_ch(n->edges[j].cclass, ch)) {
				edge = &n->edges[j];
				break;
			}
		}
		if (!edge) {
			dprintf(" reject\n");
			return 0;
		}
		dprintf(" ->");
		state = edge->dest;
	}
	if (!dfa->nodes[state].nfinals) {
		dprintf(" %u reject (end of string)\n", state);
		return 0;
	}
	dprintf(" %u accept\n", state);
	return 1;
}

/* Tests if an environment variable is set, non-empty and doesn't start
 * with either '0' or 'n'. */
static int
testenv(const char *e)
{
	char *c = getenv(e);
	return c && *c && *c != '0' && *c != 'n';
}

/* Dump a graph, showing line number (only used in debugging) */
#define DUMP(dfa) do {					\
	if (Debug) {					\
		printf("%s:%u: %s =\n",			\
			__FILE__, __LINE__, #dfa);	\
		nfa_dump(stdout, dfa, -1);		\
	}						\
    } while (0)

/*
 * MAKE_DFA(dfa, re) is the same as:
 *    dfa = nfa_dfa(make_nfa(re))
 * but with more verbosity.
 */
#define MAKE_DFA(dfa, re) do {				\
		struct nfa *nfa;			\
		const char *_re = re;			\
		dprintf("\n%s:%u: make: /%s/\n",	\
			__FILE__,__LINE__,_re);		\
		nfa = make_nfa(_re);			\
		DUMP(nfa);				\
		nfa_to_dfa(nfa);			\
		dfa = nfa;				\
		DUMP(dfa);				\
		assert_deterministic(dfa);		\
	} while (0)

int
main()
{
	Debug = testenv("DEBUG");
	{
		/* Empty pattern */
		struct nfa *dfa;
		MAKE_DFA(dfa, "");
		assert(dfa_matches(dfa, ""));
		assert(!dfa_matches(dfa, "x"));

		assert(dfa->nodes[0].nfinals == 1);
		assert(strcmp(dfa->nodes[0].finals[0], "") == 0);

		/* nfa_add_final() */
		const char * const s = "TEST";
		nfa_add_final(dfa, 0, s);
		nfa_add_final(dfa, 0, s);
		nfa_add_final(dfa, 0, s);
		assert(dfa->nodes[0].nfinals == 2);
		assert(dfa->nodes[0].finals[1] == s);

		nfa_free(dfa);
	}
	{
		/* Single character */
		struct nfa *dfa;
		MAKE_DFA(dfa, "c");
		assert(dfa_matches(dfa, "c"));
		assert(!dfa_matches(dfa, ""));
		assert(!dfa_matches(dfa, "cc"));
		assert(!dfa_matches(dfa, "cx"));
		assert(!dfa_matches(dfa, "x"));
		nfa_free(dfa);
	}
	{
		/* Sequence of characters, character classes */
		struct nfa *dfa;
		MAKE_DFA(dfa, "[a-c][a-c][a-c]");
		assert(dfa_matches(dfa, "abc"));
		assert(dfa_matches(dfa, "aaa"));
		assert(!dfa_matches(dfa, "a"));
		assert(!dfa_matches(dfa, "aaaa"));
		assert(!dfa_matches(dfa, "aad"));
		nfa_free(dfa);
	}
	{
		/* Disjunctions */
		struct nfa *dfa;
		MAKE_DFA(dfa, "a|b");
		assert(dfa_matches(dfa, "a"));
		assert(dfa_matches(dfa, "b"));
		assert(!dfa_matches(dfa, "c"));
		assert(!dfa_matches(dfa, ""));
		nfa_free(dfa);
	}
	{
		/* Kleene star */
		struct nfa *dfa;
		MAKE_DFA(dfa, "a*");
		assert(dfa_matches(dfa, ""));
		assert(dfa_matches(dfa, "a"));
		assert(dfa_matches(dfa, "aaaaaa"));
		assert(!dfa_matches(dfa, "aaaaac"));
		assert(!dfa_matches(dfa, "caaaaa"));
		nfa_free(dfa);
	}
	{
		/* Overlapping disjunctions */
		struct nfa *dfa;
		MAKE_DFA(dfa, "[a-d]x|[c-f]y");
		assert(dfa_matches(dfa, "ax"));
		assert(dfa_matches(dfa, "bx"));
		assert(dfa_matches(dfa, "cx"));
		assert(dfa_matches(dfa, "dx"));
		assert(dfa_matches(dfa, "cy"));
		assert(dfa_matches(dfa, "dy"));
		assert(dfa_matches(dfa, "ey"));
		assert(dfa_matches(dfa, "fy"));
		assert(!dfa_matches(dfa, "fx"));
		assert(!dfa_matches(dfa, "ay"));
		assert(!dfa_matches(dfa, "x"));
		assert(!dfa_matches(dfa, "cc"));
		nfa_free(dfa);
	}
	{
		struct nfa *dfa;
		MAKE_DFA(dfa, "aca*|a*ba");

		assert(dfa_matches(dfa, "ac"));
		assert(dfa_matches(dfa, "aca"));
		assert(dfa_matches(dfa, "acaa"));
		assert(dfa_matches(dfa, "ba"));
		assert(dfa_matches(dfa, "aba"));
		assert(dfa_matches(dfa, "aaba"));
		assert(!dfa_matches(dfa, "b"));
		assert(!dfa_matches(dfa, "c"));
		assert(!dfa_matches(dfa, "ca"));
		assert(!dfa_matches(dfa, "ab"));
		assert(!dfa_matches(dfa, "abca"));
		nfa_free(dfa);
	}

	return 0;
}
