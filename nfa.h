#ifndef nfa_h
#define nfa_h

#include <stdlib.h>
#include "cclass.h"

/**
 * A (non-)deterministic finite automaton.
 * This is a graph structure that can be used for an NFA or DFA.
 * Each graph is a set of nodes (possible states) each with
 * a set of character class edges to other nodes.
 * DFAs are guaranteed to have the properties of:
 *
 *   - no epsilon edges (edge.cclass != NULL)
 *   - unique edges for any character
 *     (the edge.cclass do not overlap).
 *
 * A weaker guarantee is that the 'finals' set for a node
 * has at most one member. Or, at least, its first member
 * is the most important.
 */
struct nfa {
	unsigned nnodes;
	struct node {
		unsigned nfinals;
		const void **finals;
		unsigned nedges;
		struct edge {
			cclass *cclass;
			unsigned dest;
		} *edges;
	} *nodes;
};


/**
 * Initializes existing graph storage.
 * The nfa must later be finialized by passing it to #nfa_fini().
 *
 * @param nfa	the nfa to initialize to empty
 *
 * @returns the same @a nfa
 */
struct nfa *	nfa_init(struct nfa *nfa);

/**
 * Releases content of an initialized graph structure.
 *
 * @param nfa	an nfa that had been initialied by #nfa_init().
 */
void		nfa_fini(struct nfa *nfa);

/**
 * Allocates and initializes a new, empty graph structure.
 *
 * @returns an nfa that must eventually be released by #nfa_free().
 */
struct nfa *	nfa_new(void);

/**
 * Releases all storage associated with an nfa.
 *
 * @param nfa	an nfa obtained from #nfa_new().
 */
void		nfa_free(struct nfa *nfa);

/** Adds a new, empty node to the graph.
 * @returns the new node's index into @c{g->nodes[]}. */
unsigned	nfa_new_node(struct nfa *nfa);

/**
 * Adds a value to a node's #node.finals array.
 * These values are handy for automaton users that need
 * to associate some accept states with differnt kinds of
 * accept status.
 *
 * @param g     the graph containing the node
 * @param n     the node's index
 * @param final the final value to add to the node.
 */
void		nfa_add_final(struct nfa *nfa, unsigned n, const void *final);

/**
 * Adds an epsilon edge to the graph.
 * An epsilon edge in a NFA means that the machine may
 * transit the edge at any time.
 * Normaly though, the caller can immediately convert the
 * edge into a non-epsilon by setting the #edge.cclass
 * field to a non-null pointer.
 *
 * @param g    the graph into which to add the edge
 * @param from the node index the edge leaves from
 * @param to   the node index the edge enters to
 *
 * @returns a temporary pointer to the edge
 * (It's only valid until the call to this function, because
 * realloc may adjust pointers.)
 */
struct edge *nfa_new_edge(struct nfa *nfa, unsigned from, unsigned to);

/**
 * Converts a non-deterministic graph into a deterministic one.
 * The conversion is performed in-place.
 *
 * @param nfa   the graph to make deterministic.
 */
void nfa_to_dfa(struct nfa *nfa);

#endif /* nfa_h */
