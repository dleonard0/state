#include <stdlib.h>
#include "graph.h"
#include "bitset.h"

#define TRANSINC 16
#define NODEINC 16
#define FINALINC 16

/* graph */

struct graph *
graph_init(struct graph *g)
{
	g->nnodes = 0;
	g->nodes = 0;
	return g;
}

void
graph_fini(struct graph *g)
{
	unsigned i, j;
	for (i = 0; i < g->nnodes; i++) {
		struct node *n = &g->nodes[i];
		for (j = 0; j < n->ntrans; ++j) {
			cclass_free(n->trans[j].cclass);
		}
		free(n->trans);
		free(n->finals);
	}
	free(g->nodes);
	g->nodes = 0;
	g->nnodes = 0;
}

struct graph *
graph_new()
{
	struct graph *g = malloc(sizeof *g);

	if (g) {
		graph_init(g);
	}
	return g;
}


void
graph_free(struct graph *g)
{
	if (g) {
		graph_fini(g);
		free(g);
	}
}

unsigned
graph_new_node(struct graph *g)
{
	unsigned i;
	if ((g->nnodes % NODEINC) == 0) {
		g->nodes = realloc(g->nodes,
			(g->nnodes + NODEINC) * sizeof *g->nodes);
	}
	i = g->nnodes++;
	memset(&g->nodes[i], 0, sizeof g->nodes[i]);
	return i;
}

struct transition *
graph_new_trans(struct graph *g, unsigned from, unsigned to)
{
	struct node *n = &g->nodes[from];
	struct transition *trans;
	if (n->ntrans % TRANSINC == 0) {
		n->trans = realloc(n->trans, 
			(n->ntrans + TRANSINC) * sizeof *n->trans);
	}
	trans = &n->trans[n->ntrans++];
	trans->cclass = 0;
	trans->dest = to;
	return trans;
}

void
graph_add_final(struct graph *g, unsigned i, const void *final)
{
	struct node *n = &g->nodes[i];
	unsigned j;

	/* Check if the value is already in the final set */
	for (j = 0; j < n->nfinals; ++j)
		if (n->finals[j] == final)
			return;

	if (n->nfinals % FINALINC == 0) {
		n->finals = realloc(n->finals,
			(n->nfinals + FINALINC) * sizeof *n->finals);
	}
	n->finals[n->nfinals++] = final;
}

static int
transition_is_epsilon(const struct transition *t)
{
	return !t->cclass;
}

/* 
 * Compute the epsilon-closure of the set s.
 * That's all the states reachable through zero or more epsilon transitions
 * in g from any of the states in s.
 * @param g the graph with the epsilon transitions
 * @param s the set to expand to epsilon closure
 */
void
epsilon_closure(const struct graph *g, bitset *s)
{
	unsigned ni, j;
	struct node *n;
	struct transition *t;
	bitset *tocheck = bitset_dup(s);

	while (!bitset_is_empty(tocheck)) {
		bitset_for(ni, tocheck) {
			n = &g->nodes[ni];
			bitset_remove(tocheck, ni);
			for (j = 0; j < n->ntrans; ++j) {
				t = &n->trans[j];
				if (transition_is_epsilon(t)) {
					if (bitset_insert(s, t->dest)) {
						bitset_insert(tocheck, t->dest);
					}
				}
			}
		}
	}
	bitset_free(tocheck);
}

struct equiv {
	const struct graph *nfa;
	unsigned max, avail;
	bitset **set;
};

static void
equiv_init(struct equiv *equiv, const struct graph *nfa)
{
	equiv->nfa = nfa;
	equiv->max = 0;
	equiv->avail = 0;
	equiv->set = 0;
}

static bitset *
equiv_get(struct equiv *equiv, unsigned i) {
	if (equiv->avail <= i) {
		unsigned oldavail = equiv->avail;
		while (equiv->avail <= i) equiv->avail += 32;
		equiv->set = realloc(equiv->set, 
			equiv->avail * sizeof *equiv->set);
		memset(equiv->set + oldavail, 0, 
			(equiv->avail - oldavail) * sizeof *equiv->set);
	}
	if (!equiv->set[i]) {
		equiv->set[i] = bitset_new(equiv->nfa->nnodes);
		if (i >= equiv->max)
			equiv->max = i + 1;
	}
	return equiv->set[i];
}

static void
equiv_cleanup(struct equiv *e) {
	free(e->set);
}

/* Find or create the equivalent DFA state for a set of NFA nodes. */
static unsigned
equiv_lookup(struct graph *dfa, struct equiv *equiv, const bitset *bs)
{
	unsigned i, j, n;

	/* Check to see if we've already constructed the equivalent-node */
	for (i = 0; i < equiv->max; ++i) {
		if (equiv->set[i] && bitset_cmp(equiv->set[i], bs) == 0) {
			return i;
		}
	}

	/* Haven't seen that NFA set before, so let's allocate a DFA node */
	n = graph_new_node(dfa);

	/* Merge the set of final pointers */
	bitset_for(j, bs) {
		const struct node *jnode = &equiv->nfa->nodes[j];
		for (i = 0; i < jnode->nfinals; ++i) {
			graph_add_final(dfa, n, jnode->finals[i]);
		}
	}

	bitset_copy(equiv_get(equiv, n), bs);
	return n;
}

static int
unsigned_cmp(const void *a, const void *b)
{
	unsigned aval = *(const unsigned *)a;
	unsigned bval = *(const unsigned *)b;

	return aval < bval ? -1 : aval > bval;
}

/*
 * Construct the break set of all transitions in all the
 * given nodes.
 * For example, the cclasses [p-y] and [pt] expand to the
 * interval ranges:
 *    [a-z] =  [p,z)
 *    [pt]  =  [p,q),[t,u)
 * The break set of these two cclasses is the
 * simple set union of the his and los:
 *
 *     {p,q,t,u,z}
 *
 * @param nfa            the graph from which to draw the nodes
 * @param nodes          the set of nodes from which to draw the cclasses
 * @param nbreaks_return where to store the length of the returned set
 * @return the array of breakpoints.
 */
static unsigned *
cclass_breaks(const graph *nfa, const bitset *nodes, unsigned *nbreaks_return)
{
	unsigned ni;
	unsigned i, j;

	/* Count the number of intervals */
	unsigned nintervals = 0;
	bitset_for(ni, bs) {
		const struct node *n = &nfa->nodes[ni];
		for (j = 0; j < n->ntrans; ++j) {
			const cclass *cc = n->trans[j].cclass;
			if (cc) {
				nintervals += cc->nintervals;
			}
		}
	}

	if (!nintervals) {
		/* shortcut empty set */
		*nbreaks_return = 0;
		return NULL;
	}

	/* build the non-normalized set of cclass breaks */
	unsigned *breaks = malloc(nintervals * 2 * sizeof (unsigned));
	unsigned nbreaks = 0;
	bitset_for(ni, bs) {
		const struct node *n = &nfa->nodes[ni];
		for (j = 0; j < n->ntrans; ++j) {
			const cclass *cc = n->trans[j].cclass;
			if (cc) {
				for (i = 0; i < cc->nintervals; ++i) {
					breaks[nbreaks++] = cc->interval[i].lo;
					breaks[nbreaks++] = cc->interval[i].hi;
				}
			}
		}
	}

	/* normalize into a sorted set */
	qsort(breaks, nbreaks, sizeof *breaks, unsigned_cmp);
	unsigned last = breaks[0];
	unsigned len = 1;
	for (i = 1; i < nbreaks; ++i) {
		if (breaks[i] != last) {
			last = breaks[len++] = breaks[i];
		}
	}
	*nbreaks_return = len;
	return realloc(breaks, len * sizeof *breaks);
}

/*
 * Constructs a deterministic automaton that simulates the
 * input nfa, but only has deterministic transitions (that is
 * each node has only 0 or 1 transitions for any character).
 * @param dfa an empty graph into which to store the DFA
 * @param nfa the input (non-deterministic) graph
 */
static void
make_dfa(struct graph *dfa, const struct graph *nfa)
{
	struct bitset *bs;
	struct equiv equiv;
	unsigned ei;

	equiv_init(&equiv, nfa);

	/* the initial dfa node is the epislon closure of the nfa's initial */
	bs = bitset_new(nfa->nnodes);
	bitset_insert(bs, 0);
	epsilon_closure(nfa, bs);
	equiv_lookup(dfa, &equiv, bs) /* == 0 */;
	bitset_free(bs);


	for (ei = 0; ei < dfa->nnodes; ei++) {
		const struct node *en = &dfa->nodes[ei];
		bs = equiv_get(&equiv, ei);

		unsigned nbreaks;
		unsigned *breaks = cclass_breaks(nfa, bs, &nbreaks);

		MOREEEEE

		for (ci = 0; ci < allcc->nintervals; ++ci) {
		    const unsigned lo = allcc->interval[ci].lo;
		    const unsigned hi = allcc->interval[ci].hi;
		    unsigned ch;
		    for (ch = lo; ch < hi; ++ch) {
		        unsigned di;
		    	bitset *dest = bitset_alloca(nfa->nnodes);
			bitset_for(ni, bs) {
				const struct node *n = &nfa->nodes[ni];
				for (j = 0; j < n->ntrans; ++j) {
					if (n->trans[j].cclass &&
					    cclass_contains_ch(
					        n->trans[j].cclass, ch))
					{
						bitset_insert(dest, 
							n->trans[j].dest);
					}
				}
			}
			epsilon_closure(nfa, dest);
			/* ch -> {dest} in the DFA */

			/* find or make a new node di in the dfa for {dest} */
			di = equiv_lookup(dfa, &equiv, dest);

			/* Add the ch->di to en */
			struct transition *t = NULL;
			for (j = 0; j < en->ntrans; ++j) {
				if (en->trans[j].dest == di) {
					t = &en->trans[j];
					break;
				}
			}
			if (!t) {
				t = graph_new_trans(dfa, ei, di);
				t->cclass = cclass_new();
			}
			cclass_add(t->cclass, ch, ch + 1);
		    }
		}
		cclass_free(allcc);
	}

	/* TODO: repeatedly remove duplicate states */

	equiv_cleanup(&equiv);
}

void
graph_to_dfa(struct graph *g)
{
	struct graph nfa = *g;
	graph_init(g);
	make_dfa(g, &nfa);
	graph_fini(&nfa);
}
