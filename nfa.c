#include <stdlib.h>
#include "nfa.h"
#include "bitset.h"

#define TRANSINC 16
#define NODEINC 16
#define FINALINC 16

/* graph */

struct nfa *
nfa_init(struct nfa *g)
{
	g->nnodes = 0;
	g->nodes = 0;
	return g;
}

void
nfa_fini(struct nfa *g)
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

struct nfa *
nfa_new()
{
	struct nfa *g = malloc(sizeof *g);

	if (g) {
		nfa_init(g);
	}
	return g;
}


void
nfa_free(struct nfa *g)
{
	if (g) {
		nfa_fini(g);
		free(g);
	}
}

unsigned
nfa_new_node(struct nfa *g)
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
nfa_new_trans(struct nfa *g, unsigned from, unsigned to)
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
nfa_add_final(struct nfa *g, unsigned i, const void *final)
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
epsilon_closure(const struct nfa *g, bitset *s)
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

/*
 * Equivalance set: a mapping from DFA node IDs to a set of NFA nodes.
 * We need the nfa graph so that bitsets can be allocated with the same
 * capacity as the number of nodes in the nfa.
 */
struct equiv {
	const struct nfa *nfa;
	unsigned max, avail;
	bitset **set;
};

static void
equiv_init(struct equiv *equiv, const struct nfa *nfa)
{
	equiv->nfa = nfa;
	equiv->max = 0;
	equiv->avail = 0;
	equiv->set = 0;
}

/*
 * Returns the bitset corresponding to DFA node i.
 * Allocates storage in the equiv map as needed;
 * initializes never-before requested bitsets to empty.
 */
static bitset *
equiv_get(struct equiv *equiv, unsigned i)
{
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
equiv_cleanup(struct equiv *equiv) {
	unsigned i;

	for (i = 0; i < equiv->max; ++i)
		bitset_free(equiv->set[i]);
	free(equiv->set);
}

/*
 * Find (or create new) an equivalent DFA state for a given
 * set of NFA nodes.
 * Searches the equiv map for the bitset bs.
 * Note that this may add nodes to the DFA.
 */
static unsigned
equiv_lookup(struct nfa *dfa, struct equiv *equiv, const bitset *bs)
{
	unsigned i, j, n;

	/* Check to see if we've already constructed the equivalent-node */
	for (i = 0; i < equiv->max; ++i) {
		if (equiv->set[i] && bitset_cmp(equiv->set[i], bs) == 0) {
			return i;
		}
	}

	/* Haven't seen that NFA set before, so let's allocate a DFA node */
	n = nfa_new_node(dfa);

	/* Merge the set of final pointers */
	bitset_for(j, bs) {
		const struct node *jnode = &equiv->nfa->nodes[j];
		for (i = 0; i < jnode->nfinals; ++i) {
			nfa_add_final(dfa, n, jnode->finals[i]);
		}
	}

	bitset_copy(equiv_get(equiv, n), bs);
	return n;
}

/* Unsigned integer comparator for qsort */
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
cclass_breaks(const struct nfa *nfa, const bitset *nodes, unsigned *nbreaks_return)
{
	unsigned ni;
	unsigned i, j;

	/* Count the number of intervals */
	unsigned nintervals = 0;
	bitset_for(ni, nodes) {
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

	/* Build the non-normalized set of cclass breaks
	 * by just collecting all lo and hi values */
	unsigned *breaks = malloc(nintervals * 2 * sizeof (unsigned));
	unsigned nbreaks = 0;
	bitset_for(ni, nodes) {
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

	/* Sort the array of breaks */
	qsort(breaks, nbreaks, sizeof *breaks, unsigned_cmp);

	/* Remove duplicates */
	unsigned lastbreak = breaks[0];
	unsigned newlen = 1;
	for (i = 1; i < nbreaks; ++i) {
		if (breaks[i] != lastbreak) {
			lastbreak = breaks[newlen++] = breaks[i];
		}
	}
	nbreaks = newlen;

	/* Reduce storage overhead */
	breaks = realloc(breaks, nbreaks * sizeof *breaks);

	*nbreaks_return = nbreaks;
	return breaks;
}

/*
 * Constructs a deterministic automaton that simulates the
 * input nfa, but only has deterministic transitions (that is
 * each node has only 0 or 1 transitions for any character).
 * @param dfa an empty graph into which to store the DFA
 * @param nfa the input (non-deterministic) graph
 */
static void
make_dfa(struct nfa *dfa, const struct nfa *nfa)
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

	/*
	 * Iterate ei over the unprocessed DFA nodes.
	 * Each iteration may add more DFA nodes, but it
	 * won't add duplicates.
	 */
	for (ei = 0; ei < dfa->nnodes; ei++) {
		const struct node *en = &dfa->nodes[ei];
		unsigned nbreaks, bi;
		unsigned *breaks;
		struct bitset *src;

		/* src is the set of NFA nodes corresponding to
		 * the current DFA node ei */
		src = equiv_get(&equiv, ei);

		/* We want to combine all the transition cclasses
		 * of src together, and then efficiently walk over
		 * their members. 
		 * The breaks list speeds this up because if 
		 * characters c1,c2 appear adjacent in the breaks list,
		 * then we can reason that for any cclass in any src
		 * transitions, either:
		 *   [c1,c2) is wholly within that cclass
		 *   [c1,c2) is wholly outside that cclass
		 */
		breaks = cclass_breaks(nfa, src, &nbreaks);
		for (bi = 1; bi < nbreaks; ++bi) {
			const unsigned lo = breaks[bi - 1];
			const unsigned hi = breaks[bi];
			unsigned ni, di, j;

			/* Find the set of NFA states, dest, to which
			 * the cclass [lo,hi) transitions to from the
			 * src set. We can do this by just checking for
			 * membership of lo. */
			bitset *dest = bitset_alloca(nfa->nnodes);
			bitset_for(ni, src) {
				const struct node *n = &nfa->nodes[ni];
				for (j = 0; j < n->ntrans; ++j) {
					if (n->trans[j].cclass &&
					    cclass_contains_ch(n->trans[j].cclass, lo))
					{
						bitset_insert(dest, n->trans[j].dest);
					}
				}
			}
			/* Expand the resulting dest set to its epsilon closure */
			epsilon_closure(nfa, dest);

			/* Find or make di, the DFA equivalent node for {dest} */
			di = equiv_lookup(dfa, &equiv, dest);

			/* (Recompute pointers here because nodes may have been realloced) */
			en = &dfa->nodes[ei];

			/* Create or find an existing transition from ei->di */
			struct transition *t = NULL;
			for (j = 0; j < en->ntrans; ++j) {
				if (en->trans[j].dest == di) {
					t = &en->trans[j];
					break;
				}
			}
			if (!t) {
				t = nfa_new_trans(dfa, ei, di);
				t->cclass = cclass_new();
			}

			/* Add the transition along [lo,hi) into the DFA */
			cclass_add(t->cclass, lo, hi);
		}
		free(breaks);
	}

	/* TODO: remove duplicate states */

	equiv_cleanup(&equiv);
}

void
nfa_to_dfa(struct nfa *g)
{
	struct nfa nfa = *g;
	nfa_init(g);
	make_dfa(g, &nfa);
	nfa_fini(&nfa);
}
