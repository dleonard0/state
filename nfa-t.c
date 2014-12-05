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

/* A subgraph structure used during construction
 * of test regular expressions. The subgraph represents
 * a regular expression that is part of a larger regex. */
struct subgraph {
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

static struct subgraph parse_exp(struct graph *g, const char **sp);

/* A ::= [c-c] | . | c | (E) */
static struct subgraph
parse_atom(struct graph *g, const char **sp)
{
	struct subgraph ret;
	cclass *cc = NULL;
	char ch;
	struct transition *t;
	
	ch = *(*sp)++;
	if (ch == '(') {
		ret = parse_exp(g, sp);
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
			if (hi == ']') 
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
	ret.entry = graph_new_node(g);
	ret.exit = graph_new_node(g);
	t = graph_new_trans(g, ret.entry, ret.exit);
	t->cclass = cc;
	return ret;
}

/* T ::= A? | A* | A */
static struct subgraph
parse_term(struct graph *g, const char **sp)
{
	char ch;
	struct subgraph ret = parse_atom(g, sp);

	while ((ch = **sp) == '*' || ch == '?') {
		struct subgraph sub = ret;
		ret.entry = graph_new_node(g);
		ret.exit = graph_new_node(g);
		graph_new_trans(g, ret.entry, sub.entry);
		graph_new_trans(g, sub.exit, ret.exit);
		graph_new_trans(g, sub.entry, sub.exit);
		if (ch == '*')
			graph_new_trans(g, sub.exit, sub.entry);
		++*sp;
	}
	return ret;
}

/* S ::= empty | S T */
static struct subgraph
parse_sequence(struct graph *g, const char **sp)
{
	struct subgraph ret;
	unsigned mid;

	ret.entry = graph_new_node(g);
	mid = graph_new_node(g);
	graph_new_trans(g, ret.entry, mid);
	while (**sp && **sp != '|' && **sp != ')') {
		struct subgraph next = parse_term(g, sp);
		graph_new_trans(g, mid, next.entry);
		mid = graph_new_node(g);
		graph_new_trans(g, next.exit, mid);
	}
	ret.exit = mid;
	return ret;
}

/* F ::= S  |  F '|' S */
static struct subgraph
parse_factor(struct graph *g, const char **sp)
{
	struct subgraph ret = parse_sequence(g, sp);

	while (**sp == '|') {
		struct subgraph alt = ret;
		ret.entry = graph_new_node(g);
		ret.exit = graph_new_node(g);
		graph_new_trans(g, ret.entry, alt.entry);
		graph_new_trans(g, alt.exit, ret.exit);
		++*sp; /* '|' */
		alt = parse_sequence(g, sp);
		graph_new_trans(g, ret.entry, alt.entry);
		graph_new_trans(g, alt.exit, ret.exit);
	}
	return ret;
}

/* E ::= F */
static struct subgraph
parse_exp(struct graph *g, const char **sp)
{
	return parse_factor(g, sp);
}

/* Creates an NFA graph from a test regular expression string */
static struct graph *
make_nfa(const char *s)
{
	struct subgraph res, sub;
	struct graph *g = graph_new();

	res.entry = graph_new_node(g);
	res.exit = graph_new_node(g);
	graph_add_final(g, res.exit, s);

	sub = parse_exp(g, &s);
	graph_new_trans(g, res.entry, sub.entry);
	graph_new_trans(g, sub.exit, res.exit);

	return g;
}

/** Checks a graph to see if it really is a DFA. Aborts on error */
static void
assert_deterministic(const struct graph *g)
{
	unsigned i;
	unsigned nfinals = 0;

	assert(g->nnodes > 0);
	for (i = 0; i < g->nnodes; ++i) {
		const struct node *n = &g->nodes[i];
		unsigned j;
		cclass *allcc = cclass_new();
		for (j = 0; j < n->ntrans; ++j) {
			const struct transition *t = &n->trans[j];
			assert(t->cclass); /* no epsilons */
			assert(t->dest < g->nnodes);
			/* determinism check: */
			assert(!cclass_intersects(t->cclass, allcc));
			cclass_addcc(allcc, t->cclass);
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
dfa_matches(const struct graph *g, const char *str)
{
	unsigned state = 0;
	const char *s;

	assert_deterministic(g);

	dprintf("dfa_matches: \"%s\":", str);
	for (s = str; *s; ++s) {
		char ch = *s;
		const struct node *n = &g->nodes[state];
		const struct transition *trans = 0;
		unsigned j;

		dprintf(" %u '%c'", state, ch);

		for (j = 0; j < n->ntrans; ++j) {
			if (cclass_contains_ch(n->trans[j].cclass, ch)) {
				trans = &n->trans[j];
				break;
			}
		}
		if (!trans) {
			dprintf(" reject\n");
			return 0;
		}
		dprintf(" ->");
		state = trans->dest;
	}
	if (!g->nodes[state].nfinals) {
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
	if (Debug) { 					\
		printf("%s:%u: %s =\n", 		\
			__FILE__, __LINE__, #dfa);	\
		graph_dump(stdout, dfa, -1);		\
	}						\
    } while (0)

/* 
 * MAKE_DFA(dfa, re) is the same as:
 *    dfa = graph_dfa(make_nfa(re)) 
 * but with more verbosity.
 */
#define MAKE_DFA(dfa, re) do {				\
		struct graph *nfa;			\
		const char *_re = re;			\
		dprintf("\n%s:%u: make: /%s/\n", 	\
			__FILE__,__LINE__,_re);		\
		nfa = make_nfa(_re);			\
		DUMP(nfa);				\
		graph_to_dfa(nfa);			\
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
		struct graph *dfa;
		MAKE_DFA(dfa, "");
		assert(dfa_matches(dfa, ""));
		assert(!dfa_matches(dfa, "x"));

		assert(dfa->nodes[0].nfinals == 1);
		assert(strcmp(dfa->nodes[0].finals[0], "") == 0);

		/* graph_add_final() */
		const char * const s = "TEST";
		graph_add_final(dfa, 0, s);
		graph_add_final(dfa, 0, s);
		graph_add_final(dfa, 0, s);
		assert(dfa->nodes[0].nfinals == 2);
		assert(dfa->nodes[0].finals[1] == s);

		graph_free(dfa);
	}
	{
		/* Single character */
		struct graph *dfa;
		MAKE_DFA(dfa, "c");
		assert(dfa_matches(dfa, "c"));
		assert(!dfa_matches(dfa, ""));
		assert(!dfa_matches(dfa, "cc"));
		assert(!dfa_matches(dfa, "cx"));
		assert(!dfa_matches(dfa, "x"));
		graph_free(dfa);
	}
	{
		/* Sequence of characters, character classes */
		struct graph *dfa;
		MAKE_DFA(dfa, "[a-c][a-c][a-c]");
		assert(dfa_matches(dfa, "abc"));
		assert(dfa_matches(dfa, "aaa"));
		assert(!dfa_matches(dfa, "a"));
		assert(!dfa_matches(dfa, "aaaa"));
		assert(!dfa_matches(dfa, "aad"));
		graph_free(dfa);
	}
	{
		/* Disjunctions */
		struct graph *dfa;
		MAKE_DFA(dfa, "a|b");
		assert(dfa_matches(dfa, "a"));
		assert(dfa_matches(dfa, "b"));
		assert(!dfa_matches(dfa, "c"));
		assert(!dfa_matches(dfa, ""));
		graph_free(dfa);
	}
	{
		/* Kleene star */
		struct graph *dfa;
		MAKE_DFA(dfa, "a*");
		assert(dfa_matches(dfa, ""));
		assert(dfa_matches(dfa, "a"));
		assert(dfa_matches(dfa, "aaaaaa"));
		assert(!dfa_matches(dfa, "aaaaac"));
		assert(!dfa_matches(dfa, "caaaaa"));
		graph_free(dfa);
	}
	{
		/* Overlapping disjunctions */
		struct graph *dfa;
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
		graph_free(dfa);
	}
	{
		struct graph *dfa;
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
		graph_free(dfa);
	}

	return 0;
}
