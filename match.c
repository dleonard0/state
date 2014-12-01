#include <stdlib.h>
#include "match.h"
#include "graph.h"

struct globset {
	struct graph g;
	unsigned deterministic;
	const char *error;
	stri error_position;
};

int
patterni_step(struct patterni *pi, unsigned ch)
{
	const struct node *node = &pi->dfa.nodes[pi->state];
	unsigned j;

	/* TODO replace this with a binary search, because the
	 * transition array of a dfa node will be ordered by 
	 * their (non-overlapping) cclass fields */
	for (j = 0; j < node->ntrans; ++j) {
		const struct transition *trans = &node->trans[j];
		if (cclass_contains_ch(trans->cclass, ch)) {
			pi->state = trans->dest;
			return 1;
		}
	}
	return 0;
}

const void *
patterni_accepted(struct patterni *pi)
{
	const struct node *n = &pi->dfa.nodes[pi->state];
	if (n->nfinals) {
		return n->finals[0];
	} else {
		return 0;
	}
}

/*------------------------------------------------------------
 * glob parser
 *
 *	\x			- match literal character x
 *	?                       - if not followed by ( then same as ?([!/])
 *	*                       - if not followed by ( then same as *(?)
 *	[xyz]                   - character class; similar to @(x|y|z)
 *	[!xyz]                  - inverted character class
 *	?(pattern|...)          - match 0 or 1
 *	*(pattern|...)          - match 0 or more
 *	+(pattern|...)          - match 1 or more
 *	@(pattern|...)          - match 1 of the patterns
 *	!(pattern|...)          - NOT SUPPORTED - error
 *	otherwise               - match the character exactly
 *
 */

/*
 * Intermediate subgraph during Thompson's construction method.
 * Rule of thumb: never return a subgraph with a *backwards* epislon
 * transition from final to initial. (But a forward epsilon trans between
 * the initial and final is OK).
 * This means that any subgraph returned can safely have a forward-
 * or backwards epsilon transition placed on it
 */
struct subgraph {
        unsigned initial;
	unsigned final;
	const char *error;
};
#define IS_ERROR_SUBGRAPH(subg) ((subg).error)

static struct subgraph
subgraph_error(const char *error)
{
	struct subgraph sg;
	sg.initial = 0;
	sg.final = 0;
	sg.error = error;
	return sg;
}

/*
 * Returns an empty subgraph frame, with
 * two newly allocated states, initial and final
 * serving as the entry and exit states of an expression.
 *
 *    ┌──────┐
 *    │frame │
 *    ○      ●
 *    └──────┘
 */
static struct subgraph
subgraph_frame(struct graph *g)
{
	struct subgraph sg;
	sg.initial = graph_new_node(g);
	sg.final = graph_new_node(g);
	sg.error = 0;
	return sg;
}

/*
 * Returns a subgraph with the inner subgraph simply linked.
 * This is the basis for constructing more complicated subgraphs.
 *
 *    ┌───────────────┐
 *    │box            │
 *    │   ┌───────┐   │
 *    ○─ε→○ inner ●─ε→●
 *    │   └───────┘   │
 *    └───────────────┘
 */
static struct subgraph
subgraph_box(struct graph *g, struct subgraph inner)
{
	struct subgraph box = subgraph_frame(g);
	graph_new_trans(g, box.initial, inner.initial);
	graph_new_trans(g, inner.final, box.final);
	return box;
}

static struct subgraph parse_sequence(struct graph *g, stri *i);

/*
 * Parses ?(.|..) *(.|..) +(.|..) @(.|..) !(.|..)
 *
 * First the alt ::= (seq|..|seq) is parsed:
 *    ┌──────────────┐
 *    │alt           │
 *    │   ┌─────┐    │
 *    │┌ε→○ seq ●─ε┐ │
 *    ││  └─────┘  │ │
 *    ○┼     :     ┼→●
 *    ││  ┌─────┐  │ │
 *    │└ε→○ seq ●─ε┘ │
 *    │   └─────┘    │
 *    └──────────────┘
 * Then the alt is wrapped in + * ? (@ means no change)
 *    ┌─────────────┐  ┌─────────────┐ 
 *    │+            │  │*            │  ┌─────────────┐
 *    │  ┌───ε───┐  │  │  ┌───ε───┐  │  │?            │
 *    │  ↓┌─────┐│  │  │  ↓┌─────┐│  │  │   ┌─────┐   │
 *    ○─ε→○ alt ●┴ε→●  ○┬ε→○ alt ●┴ε→●  ○┬ε→○ alt ●─ε→●
 *    │   └─────┘   │  ││  └─────┘  ↑│  ││  └─────┘  ↑│
 *    └─────────────┘  │└───────────┘│  │└───────────┘│
 *                     └─────────────┘  └─────────────┘
 *
 */
static struct subgraph
parse_group(struct graph *g, stri *i, unsigned kind)
{
	if (kind == '!') {
		return subgraph_error("!(...) is not supported");
	}
	stri_inc(*i); /* '(' */

	struct subgraph alt = subgraph_frame(g);
	while (stri_more(*i) && stri_at(*i) != ')') {
		struct subgraph seq = parse_sequence(g, i);
		if (IS_ERROR_SUBGRAPH(seq)) {
			return seq;
		}
		graph_new_trans(g, alt.initial, seq.initial);
		graph_new_trans(g, seq.final, alt.final);
	}
	if (!stri_more(*i)) {
		return subgraph_error("unclosed (");
	}
	stri_inc(*i); /* ')' */

	if (g->nodes[alt.initial].ntrans == 0) {
		/* empty alt, () */
		graph_new_trans(g, alt.initial, alt.final);
	}

	struct subgraph ret = subgraph_box(g, alt);
	switch (kind) {
	case '?':
		graph_new_trans(g, ret.initial, ret.final);
		break;
	case '*':
		graph_new_trans(g, ret.initial, ret.final);
		graph_new_trans(g, alt.final, alt.initial);
		break;
	case '+':
		graph_new_trans(g, alt.final, alt.initial);
		break;
	case '@':
		break;
	}
	return ret;
}

/* Parse a [...] character class; after the '[' has been consumed. */
static struct subgraph
parse_cclass(struct graph *g, stri *i)
{
	struct subgraph sg = subgraph_frame(g);
	cclass *cc = cclass_new();
	int invert = 0;
	struct transition *trans;
	trans = graph_new_trans(g, sg.initial, sg.final);
	trans->cclass = cc;

	if (stri_more(*i) && stri_at(*i) == '!') {
		invert = 1;
		stri_inc(*i);
	}
	if (stri_more(*i) && stri_at(*i) == ']') {
		stri_inc(*i);
		cclass_add(cc, ']', ']'+1);
	}
	for (;;) {
		unsigned lo, hi;
		if (!stri_more(*i)) {
			return subgraph_error("unclosed [");
		}
		lo = stri_utf8_inc(i);
		if (lo == ']')
			break;
		if (stri_more(*i) && lo == '\\')
			lo = stri_utf8_inc(i);
		if (stri_more(*i) && stri_at(*i) == '-') {
			stri_inc(*i);
			if (!stri_more(*i)) {
				return subgraph_error("unclosed [");
			}
			hi = stri_utf8_inc(i);
			if (stri_more(*i) && hi == '\\')
				hi = stri_utf8_inc(i);
		} else {
			hi = lo;
		}
		if (hi < lo) {
			return subgraph_error("bad character class");
		}
		if (lo == '/' || hi == '/') {
			return subgraph_error(
				"cannot have / in character class");
		}
		if (lo < '/' && '/' < hi) {
			/* remove / from implied range */
			cclass_add(cc, lo, '/');
			cclass_add(cc, '/' + 1, hi + 1);
		} else {
			cclass_add(cc, lo, hi + 1);
		}
	}
	if (invert) {
		/* add / now so that it is removed during inversion */
		cclass_add(cc, '/', '/' + 1);
		trans->cclass = cclass_invert(cc);
		cclass_free(cc);
		cc = trans->cclass;
	}
	return sg;
}

/* create the cclass corresponding to the glob "?" */
static cclass *
question_cclass()
{
	cclass *cc = cclass_new();
	cclass_add(cc, 1, '/');
	cclass_add(cc, '/' + 1, MAXCHAR);
	return cc;
}

/* parses a non-sequence expression */
static struct subgraph
parse_atom(struct graph *g, stri *i)
{
	/* assert(stri_more(*i)); */
	struct subgraph sg;
	unsigned ch = stri_utf8_inc(i);
	cclass *cc;

	if (stri_more(*i) && stri_at(*i) == '(' &&
		(ch == '?' || ch == '*' || ch == '+' || ch == '@' || ch == '!'))
	{
		return parse_group(g, ch, i);
	}
	if (stri_more(*i) && ch == '[') {
		return parse_cclass(g, i);
	}
	if (ch == '*') {
		sg = subgraph_frame(g);
		struct subgraph q = subgraph_frame(g); /* The "?" subgraph */
		cc = question_cclass();
		graph_new_trans(g, sg.initial, q.initial);
		graph_new_trans(g, q.initial, q.final)->cclass = cc;
		graph_new_trans(g, q.final, sg.final);
		graph_new_trans(g, q.initial, q.final);
		graph_new_trans(g, q.final, q.initial);
		return sg;
	}

	if (ch == '?') {
		cc = question_cclass();
	} else {
		cc = cclass_new();
		if (stri_more(*i) && ch == '\\') {
			ch = stri_utf8_inc(i);
		}
		cclass_add(cc, ch, ch + 1);
	}
	sg = subgraph_frame(g);
	graph_new_trans(g, sg.initial, sg.final)->cclass = cc;
	return sg;
}

/*
 * Parses a sequence of glob atoms, abandoning if
 * we hit a '|' or ')'.
 *    ┌──────────────────────────────────────────┐
 *    │seq                                       │
 *    │   ┌──────┐   ┌──────┐         ┌─────┐    │
 *    ○─ε→○ atom ●─ε→○ atom ●─ // ─ ε→○ atom ●─ε→●
 *    │   └──────┘   └──────┘         └─────┘    │
 *    └──────────────────────────────────────────┘
 */
static struct subgraph
parse_sequence(struct graph *g, stri *i)
{
	struct subgraph seq = subgraph_frame(g);
	unsigned last = seq.initial;

	while (stri_more(*i)) {
		char ch = stri_at(*i);
		if (ch == '|' || ch == ')') {
			break;
		}

		struct subgraph atom = parse_atom(g, i);
		if (IS_ERROR_SUBGRAPH(atom)) {
			return atom;
		}
		graph_new_trans(g, last, atom.initial);
		last = atom.final;
	}
	graph_new_trans(g, last, seq.final);
	return seq;
}

struct pattern *
pattern_new(const str *glob)
{
	struct pattern *pat = malloc(sizeof *pattern);
	graph_init(&pat->dfa);

	struct graph *g = graph_new();
	struct subgraph outer = subgraph_frame(g);

	stri ip = stri_str(glob);
	struct subgraph seq = parse_sequence(g, &ip);
	if (IS_ERROR_SUBGRAPH(seq)) {
		pat->error = seq.error;
		pat->error_position = ip;
		graph_free(g);
		return pat;
	}
	graph_new_trans(g, outer.initial, seq.initial);
	graph_new_trans(g, seq.final, outer.final);
	g->nodes[outer.final].final = 1;

	/* Convert the NFA g into a DFA */
	struct graph *dfa = graph_dfa(g);
	pat->dfa = *dfa;
	free(dfa);

	pat->error = 0;
	pat->error_position = stri_str(0);
	graph_free(g);

	return pattern;
}

void
pattern_free(struct pattern *pattern)
{
	graph_fini(&pattern->dfa);
	free(pattern);
}

/*------------------------------------------------------------
 * matcher
 */

struct matcher {
	const struct pattern *pattern;
	const struct generator *generator;
	void *generator_context;
	struct match *matches, **current;
};

struct matcher *
matcher_new(const struct pattern *pattern,
	    const struct generator *generator, void *context)
{
	struct matcher *matcher;
	struct match *m;

	/* The initial match list contains the deferred empty string */
	m = malloc(sizeof *m);
	m->next = 0;
	m->str = 0;
	m->stri = stri_str(0);
	m->flags = MATCH_DEFERRED;
	
	matcher = malloc(sizeof *matcher);
	matcher->pattern = pattern;
	matcher->generator = generator;
	matcher->generator_context = context;
	matcher->current = &matcher->matches;
	matcher->matches = m;

	return matcher;
}

str *
matcher_next(struct matcher *matcher)
{
	struct match *m, **mp;

again:
	/* Return the next fully matched string */
	while ((m = *matcher->current)) {
		if (!stri_more(m->stri)) {
			if (m->flags & MATCH_DEFERRED) {
				/* expand string at defer point */
				struct match *generated = 
					matcher->generator->generate(m->str,
						matcher->generator_context);
				struct match **tail = &generated;

				while (*tail) tail = &(*tail)->next;
				*tail = m->next;
				*matcher->current = generated;
				str_free(m->str);
				free(m);
			} else {
				/* complete match */
				str *result = m->str;
				*matcher->current = m->next;
				free(m);
				return result;
			}
		} else {
			matcher->current = &m->next;
		}
	}

	if (!stri_more(matcher->patterni)) {
		/* pattern exhausted */
		return 0;
	}

	/* Remove matches that couldn't match and increment patterni */
	// TODO pattern_step(matcher);
	TODO

	/* Prepare to walk the new list of matches */
	matcher->current = &matcher->matches;
	goto again;
}

void
matcher_free(struct matcher *matcher)
{
	struct match *m, *mnext;

	if (matcher->generator->free) {
	    matcher->generator->free(matcher->generator_context);
	}
	str_free(pattern);
	mnext = matcher->matches;
	while ((m = mnext)) {
		mnext = m->next;
		str_free(m->str);
		free(m);
	}
	free(matcher);
}
